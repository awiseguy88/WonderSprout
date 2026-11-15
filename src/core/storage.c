#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define STORAGE_MAGIC "WSV1"

static const char *k_temp_suffix = ".tmp";

/* Atomically replace path with tmp. On Windows uses MoveFileExW with
   MOVEFILE_REPLACE_EXISTING so an existing destination is always replaced
   (CRT rename() rejects an existing destination). Falls back to deleting the
   destination first, then moving. Returns 0 on success, -1 on failure. */
static int replace_file(const char *tmp, const char *path) {
#ifdef _WIN32
    /* Convert narrow paths to wide for the W API. */
    int wtmp = MultiByteToWideChar(CP_UTF8, 0, tmp, -1, NULL, 0);
    int wpath = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wtmp <= 0 || wtmp > MAX_PATH || wpath <= 0 || wpath > MAX_PATH) {
        return -1;
    }
    wchar_t wt[MAX_PATH], wp[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, tmp, -1, wt, wtmp);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, wpath);

    if (MoveFileExW(wt, wp, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return 0;
    }
    /* Fallback: remove destination, then move. */
    DeleteFileW(wp);
    if (MoveFileExW(wt, wp, MOVEFILE_WRITE_THROUGH)) {
        return 0;
    }
    return -1;
#else
    if (rename(tmp, path) != 0) return -1;
    return 0;
#endif
}

/* Read the v1 payload (pre-garden fields) into *out, leaving garden/session
   at defaults. Caller must have already read magic + version (v1) and have
   called progress_init(out). Returns 0 on success. */
static int read_v1_payload(FILE *f, Progress *out) {
    int good = 1;
    good &= (fread(out->completed, sizeof(out->completed[0]), ACTIVITY_COUNT, f) == ACTIVITY_COUNT);
    good &= (fread(out->stars, sizeof(out->stars[0]), ACTIVITY_COUNT, f) == ACTIVITY_COUNT);
    good &= (fread(&out->pathIndex, sizeof(out->pathIndex), 1, f) == 1);
    good &= (fread(out->stickers, sizeof(out->stickers[0]), STICKER_COUNT, f) == STICKER_COUNT);
    good &= (fread(&out->muted, sizeof(out->muted), 1, f) == 1);
    good &= (fread(&out->reduceMotion, sizeof(out->reduceMotion), 1, f) == 1);
    return good ? 0 : -1;
}

/* Read the v2 payload (v1 fields + garden/session) into *out.
   Caller must have already read magic + version (v2) and have
   called progress_init(out). Returns 0 on success. */
static int read_v2_payload(FILE *f, Progress *out) {
    if (read_v1_payload(f, out) != 0) return -1;
    int good = 1;
    good &= (fread(out->garden, sizeof(out->garden[0]), GARDEN_SLOTS, f) == GARDEN_SLOTS);
    good &= (fread(&out->sessionCount, sizeof(out->sessionCount), 1, f) == 1);
    good &= (fread(&out->sessionActive, sizeof(out->sessionActive), 1, f) == 1);
    good &= (fread(&out->sessionDone, sizeof(out->sessionDone), 1, f) == 1);
    good &= (fread(&out->seedsAvailable, sizeof(out->seedsAvailable), 1, f) == 1);
    return good ? 0 : -1;
}

int storage_load(const char *path, Progress *out) {
    if (!path || !out) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) { progress_init(out); return -1; }

    int ok = -1;
    char magic[5] = {0};
    int version = 0;

    if (fread(magic, 1, 4, f) == 4 && memcmp(magic, STORAGE_MAGIC, 4) == 0 &&
        fread(&version, sizeof(version), 1, f) == 1) {

        Progress p;
        progress_init(&p);
        p.version = PROGRESS_VERSION;

        if (version == PROGRESS_VERSION) {
            /* Current (v2) format */
            if (read_v2_payload(f, &p) == 0) { *out = p; ok = 0; }
            else { progress_init(out); ok = -1; }
        } else if (version == 1) {
            /* v1 migration: load old fields, keep garden/session at defaults */
            if (read_v1_payload(f, &p) == 0) { *out = p; ok = 0; }
            else { progress_init(out); ok = -1; }
        } else {
            /* Unknown version */
            progress_init(out);
            ok = -1;
        }
    } else {
        progress_init(out);
        ok = -1;
    }

    fclose(f);
    return ok;
}

int storage_save(const char *path, const Progress *p) {
    if (!path || !p) return -1;

    char tmp[4096];
    int n = snprintf(tmp, sizeof(tmp), "%s%s", path, k_temp_suffix);
    if (n < 0 || n >= (int)sizeof(tmp)) return -1;

    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;

    int ok = 0;
    const char *magic = STORAGE_MAGIC;
    int version = PROGRESS_VERSION;

    if (fwrite(magic, 1, 4, f) == 4 &&
        fwrite(&version, sizeof(version), 1, f) == 1 &&
        fwrite(p->completed, sizeof(p->completed[0]), ACTIVITY_COUNT, f) == ACTIVITY_COUNT &&
        fwrite(p->stars, sizeof(p->stars[0]), ACTIVITY_COUNT, f) == ACTIVITY_COUNT &&
        fwrite(&p->pathIndex, sizeof(p->pathIndex), 1, f) == 1 &&
        fwrite(p->stickers, sizeof(p->stickers[0]), STICKER_COUNT, f) == STICKER_COUNT &&
        fwrite(&p->muted, sizeof(p->muted), 1, f) == 1 &&
        fwrite(&p->reduceMotion, sizeof(p->reduceMotion), 1, f) == 1 &&
        /* v2 garden/session fields */
        fwrite(p->garden, sizeof(p->garden[0]), GARDEN_SLOTS, f) == GARDEN_SLOTS &&
        fwrite(&p->sessionCount, sizeof(p->sessionCount), 1, f) == 1 &&
        fwrite(&p->sessionActive, sizeof(p->sessionActive), 1, f) == 1 &&
        fwrite(&p->sessionDone, sizeof(p->sessionDone), 1, f) == 1 &&
        fwrite(&p->seedsAvailable, sizeof(p->seedsAvailable), 1, f) == 1) {
        ok = 0;
    } else {
        ok = -1;
    }

    if (fclose(f) != 0) ok = -1;
    if (ok == 0 && replace_file(tmp, path) != 0) ok = -1;
    if (ok != 0) remove(tmp);
    return ok;
}

int storage_reset(const char *path) {
    Progress def;
    progress_init(&def);
    return storage_save(path, &def);
}