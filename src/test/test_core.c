#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "../core/curriculum.h"
#include "../core/progress.h"
#include "../core/storage.h"
#include "../core/audio_synth.h"

static int tests_run = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    printf("  [TEST] %s ... ", #name); \
    int _r = test_##name(); \
    tests_run++; \
    if (_r) { printf("PASS\n"); } else { printf("FAIL\n"); tests_failed++; } \
} while (0)

#define ASSERT(cond) do { if (!(cond)) { \
    fprintf(stderr, "\n    ASSERT FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
    return 0; } } while (0)

static int test_curriculum_unique_ids(void) {
    for (int i = 0; i < ACTIVITY_COUNT; i++) {
        for (int j = i + 1; j < ACTIVITY_COUNT; j++) {
            ASSERT(strcmp(k_activities[i].id, k_activities[j].id) != 0);
        }
    }
    return 1;
}

static int test_curriculum_path_valid(void) {
    for (size_t i = 0; i < k_pathLen; i++) {
        int idx = activity_index_of(k_learningPath[i]);
        ASSERT(idx >= 0);
    }
    return 1;
}

static int test_curriculum_domain_coverage(void) {
    int seen[DOMAIN_COUNT] = {0};
    for (int i = 0; i < ACTIVITY_COUNT; i++) {
        ASSERT(k_activities[i].domain >= 0 && k_activities[i].domain < DOMAIN_COUNT);
        seen[k_activities[i].domain] = 1;
    }
    for (int d = 0; d < DOMAIN_COUNT; d++) ASSERT(seen[d] == 1);
    return 1;
}

static int test_progress_init(void) {
    Progress p;
    progress_init(&p);
    ASSERT(p.version == PROGRESS_VERSION);
    ASSERT(p.pathIndex == 0);
    ASSERT(progress_star_total(&p) == 0);
    ASSERT(progress_sticker_count(&p) == 0);
    for (int i = 0; i < ACTIVITY_COUNT; i++) {
        ASSERT(p.completed[i] == 0);
        ASSERT(p.stars[i] == 0);
    }
    return 1;
}

static int test_progress_unlock_and_complete(void) {
    Progress p; progress_init(&p);
    int first = activity_index_of(k_learningPath[0]);
    int second = activity_index_of(k_learningPath[1]);
    ASSERT(first >= 0 && second >= 0);
    ASSERT(progress_is_unlocked(&p, first) == 1);
    ASSERT(progress_is_unlocked(&p, second) == 0);

    int stars = progress_complete(&p, first, 3);
    ASSERT(stars == 3);
    ASSERT(p.completed[first] == 1);
    ASSERT(p.stars[first] == 3);
    ASSERT(p.pathIndex == 1);
    ASSERT(progress_is_unlocked(&p, second) == 1);
    return 1;
}

static int test_progress_replay_no_regress(void) {
    Progress p; progress_init(&p);
    int first = activity_index_of(k_learningPath[0]);
    progress_complete(&p, first, 3);
    int idxAfter = p.pathIndex;
    progress_complete(&p, first, 1);
    ASSERT(p.pathIndex == idxAfter);
    ASSERT(p.stars[first] == 3); /* keep best score */
    return 1;
}

static int test_progress_stickers(void) {
    Progress p; progress_init(&p);
    /* Complete first 5 path steps with 3 stars each -> 15 stars */
    for (int i = 0; i < 5 && i < (int)k_pathLen; i++) {
        int idx = activity_index_of(k_learningPath[i]);
        progress_complete(&p, idx, 3);
    }
    ASSERT(progress_star_total(&p) >= 15);
    unsigned int mask = progress_sticker_mask(&p);
    ASSERT(mask & (1u << 0)); /* >=5 */
    ASSERT(mask & (1u << 1)); /* >=15 */
    return 1;
}

static int test_progress_path_complete(void) {
    Progress p; progress_init(&p);
    for (size_t i = 0; i < k_pathLen; i++) {
        int idx = activity_index_of(k_learningPath[i]);
        ASSERT(progress_is_unlocked(&p, idx) == 1);
        progress_complete(&p, idx, 2);
    }
    ASSERT(p.pathIndex == (int)k_pathLen);
    unsigned int mask = progress_sticker_mask(&p);
    ASSERT(mask & (1u << 9)); /* path complete sticker */
    return 1;
}

static int test_storage_roundtrip(void) {
    const char *path = "/tmp/wondersprout_test_progress.dat";
    Progress p; progress_init(&p);
    int a = activity_index_of(k_learningPath[0]);
    progress_complete(&p, a, 2);
    p.muted = 1;
    ASSERT(storage_save(path, &p) == 0);

    Progress q; progress_init(&q);
    int r = storage_load(path, &q);
    ASSERT(r == 0);
    ASSERT(q.version == PROGRESS_VERSION);
    ASSERT(q.pathIndex == p.pathIndex);
    ASSERT(q.muted == 1);
    ASSERT(q.stars[a] == p.stars[a]);
    ASSERT(q.completed[a] == 1);

    /* Save twice to the same path: the second save must replace the first.
       This is the regression case that fails with CRT rename() on Windows
       when the destination already exists. */
    int b = activity_index_of(k_learningPath[1]);
    progress_complete(&p, b, 3);
    p.muted = 0;
    ASSERT(storage_save(path, &p) == 0);
    Progress q2; progress_init(&q2);
    ASSERT(storage_load(path, &q2) == 0);
    ASSERT(q2.pathIndex == p.pathIndex);
    ASSERT(q2.muted == 0);
    ASSERT(q2.stars[b] == 3);
    ASSERT(q2.completed[b] == 1);

    /* corrupt -> defaults */
    FILE *f = fopen(path, "wb");
    ASSERT(f != NULL);
    fprintf(f, "junk");
    fclose(f);
    Progress bad; progress_init(&bad);
    bad.pathIndex = 99;
    r = storage_load(path, &bad);
    ASSERT(r == -1);
    ASSERT(bad.pathIndex == 0); /* defaults */
    remove(path);
    return 1;
}

static int test_storage_reset_persists(void) {
    const char *path = "/tmp/wondersprout_test_reset.dat";
    Progress p; progress_init(&p);
    int a = activity_index_of(k_learningPath[0]);
    progress_complete(&p, a, 3);
    ASSERT(storage_save(path, &p) == 0);

    /* Reset must persist: saving defaults over an existing file. */
    ASSERT(storage_reset(path) == 0);
    Progress q; progress_init(&q);
    q.pathIndex = 99;
    ASSERT(storage_load(path, &q) == 0);
    ASSERT(q.pathIndex == 0);
    ASSERT(q.completed[a] == 0);
    ASSERT(progress_star_total(&q) == 0);
    remove(path);
    return 1;
}

static int test_storage_missing(void) {
    Progress p; progress_init(&p);
    p.pathIndex = 5;
    int r = storage_load("/tmp/wondersprout_no_such_file_12345.dat", &p);
    ASSERT(r == -1);
    ASSERT(p.pathIndex == 0);
    return 1;
}

static int test_audio_synth(void) {
    ToneBuffer t = synth_tone(440.0f, 0.2f, 44100);
    ASSERT(t.samples != NULL);
    ASSERT(t.n == (int)(0.2f * 44100));
    for (int i = 0; i < t.n; i++) {
        ASSERT(isfinite(t.samples[i]));
        ASSERT(t.samples[i] >= -1.5f && t.samples[i] <= 1.5f);
    }
    tone_buffer_free(&t);

    float freqs[3] = {523.25f, 659.25f, 783.99f};
    ToneBuffer c = synth_chord(freqs, 3, 0.1f, 44100);
    ASSERT(c.samples != NULL && c.n > 0);
    tone_buffer_free(&c);

    float nf[3] = {440.0f, 550.0f, 660.0f};
    float nd[3] = {0.1f, 0.1f, 0.1f};
    ToneBuffer n = synth_notes(nf, nd, 3, 44100);
    ASSERT(n.samples != NULL && n.n == (int)(0.3f * 44100));
    tone_buffer_free(&n);

    ToneBuffer s = synth_star(44100);
    ASSERT(s.samples != NULL && s.n > 0);
    tone_buffer_free(&s);

    ToneBuffer e = synth_error(44100);
    ASSERT(e.samples != NULL && e.n > 0);
    tone_buffer_free(&e);

    ToneBuffer ck = synth_click(44100);
    ASSERT(ck.samples != NULL && ck.n > 0);
    tone_buffer_free(&ck);

    return 1;
}

/* ---------- Garden / session tests ---------- */

static int test_progress_session(void) {
    Progress p; progress_init(&p);
    ASSERT(p.sessionActive == 0);
    ASSERT(p.sessionDone == 0);
    ASSERT(progress_session_ready(&p) == 0);

    progress_start_session(&p);
    ASSERT(p.sessionActive == 1);
    ASSERT(p.sessionDone == 0);

    /* Complete SESSION_GOAL activities -> session ready */
    int first = activity_index_of(k_learningPath[0]);
    progress_complete(&p, first, 3);
    ASSERT(p.sessionDone == 1);
    ASSERT(progress_session_ready(&p) == 0);

    int second = activity_index_of(k_learningPath[1]);
    progress_complete(&p, second, 2);
    ASSERT(p.sessionDone == 2);
    ASSERT(progress_session_ready(&p) == 0);

    int third = activity_index_of(k_learningPath[2]);
    progress_complete(&p, third, 1);
    ASSERT(p.sessionDone == 3);
    ASSERT(progress_session_ready(&p) == 1);

    return 1;
}

static int test_progress_finish_session(void) {
    Progress p; progress_init(&p);

    /* Plant a seed, then finish a session -> it should advance one stage. */
    p.seedsAvailable = 1;
    ASSERT(progress_plant_seed(&p, 0, PLANT_SUNFLOWER) == 1);
    ASSERT(p.garden[0].plantType == PLANT_SUNFLOWER);
    ASSERT(p.garden[0].stage == 0);
    ASSERT(progress_garden_count(&p) == 1);
    ASSERT(progress_garden_empty_slots(&p) == GARDEN_SLOTS - 1);

    /* Start + finish a session -> plant advances to stage 1, seed granted. */
    progress_start_session(&p);
    int a = activity_index_of(k_learningPath[0]);
    progress_complete(&p, a, 3); /* sessionDone=1 */
    int b = activity_index_of(k_learningPath[1]);
    progress_complete(&p, b, 3); /* sessionDone=2 */
    int c = activity_index_of(k_learningPath[2]);
    progress_complete(&p, c, 3); /* sessionDone=3 -> ready */
    ASSERT(progress_session_ready(&p) == 1);

    int sessionsBefore = p.sessionCount;
    int seedsBefore = p.seedsAvailable;
    progress_finish_session(&p);
    ASSERT(p.sessionCount == sessionsBefore + 1);
    ASSERT(p.seedsAvailable == seedsBefore + 1);
    ASSERT(p.sessionActive == 0);
    ASSERT(p.sessionDone == 0);
    ASSERT(p.garden[0].stage == 1); /* advanced one stage */

    return 1;
}

static int test_progress_plant_seed(void) {
    Progress p; progress_init(&p);
    ASSERT(progress_garden_count(&p) == 0);
    ASSERT(progress_garden_empty_slots(&p) == GARDEN_SLOTS);
    ASSERT(progress_garden_empty(&p) == 1);

    /* No seeds -> cannot plant */
    ASSERT(progress_plant_seed(&p, 0, PLANT_TULIP) == 0);
    ASSERT(progress_garden_count(&p) == 0);

    /* Grant a seed -> plant succeeds */
    p.seedsAvailable = 1;
    ASSERT(progress_plant_seed(&p, 2, PLANT_CARROT) == 1);
    ASSERT(p.garden[2].plantType == PLANT_CARROT);
    ASSERT(p.garden[2].stage == 0);
    ASSERT(p.seedsAvailable == 0);
    ASSERT(progress_garden_count(&p) == 1);
    ASSERT(progress_garden_empty_slots(&p) == GARDEN_SLOTS - 1);
    ASSERT(progress_garden_empty(&p) == 0);

    /* Slot already occupied */
    p.seedsAvailable = 1;
    ASSERT(progress_plant_seed(&p, 2, PLANT_TULIP) == 0);
    /* Invalid slot */
    ASSERT(progress_plant_seed(&p, -1, PLANT_TULIP) == 0);
    ASSERT(progress_plant_seed(&p, GARDEN_SLOTS, PLANT_TULIP) == 0);
    /* Invalid plant type */
    ASSERT(progress_plant_seed(&p, 0, 99) == 0);

    return 1;
}

static int test_storage_garden_roundtrip(void) {
    const char *path = "/tmp/wondersprout_test_garden.dat";
    Progress p; progress_init(&p);
    p.seedsAvailable = 2;
    ASSERT(progress_plant_seed(&p, 0, PLANT_SUNFLOWER) == 1);
    ASSERT(progress_plant_seed(&p, 3, PLANT_TULIP) == 1);
    p.garden[0].stage = 2;
    p.sessionCount = 5;
    ASSERT(storage_save(path, &p) == 0);

    Progress q; progress_init(&q);
    ASSERT(storage_load(path, &q) == 0);
    ASSERT(q.sessionCount == 5);
    ASSERT(q.garden[0].plantType == PLANT_SUNFLOWER);
    ASSERT(q.garden[0].stage == 2);
    ASSERT(q.garden[3].plantType == PLANT_TULIP);
    ASSERT(q.seedsAvailable == 0); /* 2 planted, 2 granted */
    ASSERT(progress_garden_count(&q) == 2);

    remove(path);
    return 1;
}

static int test_storage_v1_migration(void) {
    /* Simulate a v1 save (no garden/session fields) and verify it loads
       with defaults for the new fields. */
    const char *path = "/tmp/wondersprout_test_v1.dat";
    Progress p; progress_init(&p);
    int a = activity_index_of(k_learningPath[0]);
    progress_complete(&p, a, 2);
    p.muted = 1;

    /* Write a v1-format file manually (magic + version=1 + old fields). */
    FILE *f = fopen(path, "wb");
    ASSERT(f != NULL);
    const char *magic = "WSV1";
    int v1 = 1;
    fwrite(magic, 1, 4, f);
    fwrite(&v1, sizeof(v1), 1, f);
    fwrite(p.completed, sizeof(p.completed[0]), ACTIVITY_COUNT, f);
    fwrite(p.stars, sizeof(p.stars[0]), ACTIVITY_COUNT, f);
    fwrite(&p.pathIndex, sizeof(p.pathIndex), 1, f);
    fwrite(p.stickers, sizeof(p.stickers[0]), STICKER_COUNT, f);
    fwrite(&p.muted, sizeof(p.muted), 1, f);
    fwrite(&p.reduceMotion, sizeof(p.reduceMotion), 1, f);
    fclose(f);

    Progress q; progress_init(&q);
    ASSERT(storage_load(path, &q) == 0);
    ASSERT(q.version == PROGRESS_VERSION);
    ASSERT(q.completed[a] == 1);
    ASSERT(q.pathIndex == p.pathIndex);
    ASSERT(q.muted == 1);
    /* New fields default */
    ASSERT(q.sessionCount == 0);
    ASSERT(q.sessionActive == 0);
    ASSERT(q.sessionDone == 0);
    ASSERT(q.seedsAvailable == 0);
    ASSERT(progress_garden_count(&q) == 0);

    remove(path);
    return 1;
}

int main(void) {
    printf("WonderSprout core tests\n");
    TEST(curriculum_unique_ids);
    TEST(curriculum_path_valid);
    TEST(curriculum_domain_coverage);
    TEST(progress_init);
    TEST(progress_unlock_and_complete);
    TEST(progress_replay_no_regress);
    TEST(progress_stickers);
    TEST(progress_path_complete);
    TEST(progress_session);
    TEST(progress_finish_session);
    TEST(progress_plant_seed);
    TEST(storage_roundtrip);
    TEST(storage_missing);
    TEST(storage_reset_persists);
    TEST(storage_garden_roundtrip);
    TEST(storage_v1_migration);
    TEST(audio_synth);

    printf("\n%d/%d tests passed\n", tests_run - tests_failed, tests_run);
    return tests_failed ? 1 : 0;
}
