#include "progress.h"
#include <string.h>

void progress_init(Progress *p) {
    if (!p) return;
    p->version = PROGRESS_VERSION;
    memset(p->completed, 0, sizeof(p->completed));
    memset(p->stars, 0, sizeof(p->stars));
    memset(p->stickers, 0, sizeof(p->stickers));
    p->pathIndex = 0;
    p->muted = 0;
    p->reduceMotion = 0;

    /* Garden / session defaults */
    memset(p->garden, 0, sizeof(p->garden));
    p->sessionCount = 0;
    p->sessionActive = 0;
    p->sessionDone = 0;
    p->seedsAvailable = 0;
}

static int path_position_of_act(int actIdx) {
    return activity_path_position(actIdx);
}

int progress_is_unlocked(const Progress *p, int actIdx) {
    if (!p || actIdx < 0 || actIdx >= ACTIVITY_COUNT) return 0;
    int pos = path_position_of_act(actIdx);
    if (pos < 0) return 1;            /* free-play activity: always open */
    if (pos == 0) return 1;           /* first path step is always open */
    int prevIdx = activity_index_of(k_learningPath[pos - 1]);
    if (prevIdx < 0) return 1;
    return p->completed[prevIdx] ? 1 : 0;
}

int progress_complete(Progress *p, int actIdx, int score) {
    if (!p || actIdx < 0 || actIdx >= ACTIVITY_COUNT) return 0;
    if (score < 0) score = 0;
    if (score > 3) score = 3;

    p->completed[actIdx] = 1;
    if ((int)p->stars[actIdx] < score) p->stars[actIdx] = (unsigned char)score;

    /* If this was the current path step, walk pathIndex forward over
       anything already completed. */
    int pos = path_position_of_act(actIdx);
    if (pos >= 0 && pos == p->pathIndex) {
        while (p->pathIndex < (int)k_pathLen) {
            int nxt = activity_index_of(k_learningPath[p->pathIndex]);
            if (nxt < 0 || !p->completed[nxt]) break;
            p->pathIndex++;
        }
    }

    /* Track session progress */
    if (p->sessionActive) {
        p->sessionDone++;
    }
    return (int)p->stars[actIdx];
}

int progress_next_path_step(const Progress *p) {
    if (!p) return -1;
    if (p->pathIndex < 0 || p->pathIndex >= (int)k_pathLen) return -1;
    return activity_index_of(k_learningPath[p->pathIndex]);
}

int progress_star_total(const Progress *p) {
    int s = 0;
    for (int i = 0; i < ACTIVITY_COUNT; i++) s += p->stars[i];
    return s;
}

int progress_sticker_count(const Progress *p) {
    int c = 0;
    for (int i = 0; i < STICKER_COUNT; i++) if (p->stickers[i]) c++;
    return c;
}

unsigned int progress_sticker_mask(const Progress *p) {
    unsigned int mask = 0;
    int total = progress_star_total(p);

    if (total >= 5)  mask |= 1u << 0;
    if (total >= 15) mask |= 1u << 1;
    if (total >= 25) mask |= 1u << 2;
    if (total >= 35) mask |= 1u << 3;
    if (total >= 45) mask |= 1u << 4;

    int domDone[DOMAIN_COUNT] = {0};
    int domTotal[DOMAIN_COUNT] = {0};
    for (int i = 0; i < ACTIVITY_COUNT; i++) {
        domTotal[k_activities[i].domain]++;
        if (p->completed[i]) domDone[k_activities[i].domain]++;
    }
    for (int d = 0; d < DOMAIN_COUNT; d++) {
        if (domDone[d] > 0 && domDone[d] == domTotal[d]) {
            mask |= 1u << (5 + d);
        }
    }

    int pathDone = 1;
    for (size_t i = 0; i < k_pathLen; i++) {
        int idx = activity_index_of(k_learningPath[i]);
        if (idx < 0 || !p->completed[idx]) { pathDone = 0; break; }
    }
    if (pathDone) mask |= 1u << 9;

    return mask;
}

/* ---------- Garden / session ---------- */

void progress_start_session(Progress *p) {
    if (!p) return;
    p->sessionActive = 1;
    p->sessionDone = 0;
}

int progress_session_ready(const Progress *p) {
    if (!p) return 0;
    return p->sessionDone >= SESSION_GOAL;
}

void progress_finish_session(Progress *p) {
    if (!p) return;
    /* Water the garden: advance every planted plant one stage (up to bloom). */
    for (int i = 0; i < GARDEN_SLOTS; i++) {
        if (p->garden[i].plantType != PLANT_NONE && p->garden[i].stage < 3) {
            p->garden[i].stage++;
        }
    }
    p->sessionCount++;
    p->seedsAvailable++;
    p->sessionActive = 0;
    p->sessionDone = 0;
}

int progress_plant_seed(Progress *p, int slot, int plantType) {
    if (!p || slot < 0 || slot >= GARDEN_SLOTS) return 0;
    if (plantType < PLANT_SUNFLOWER || plantType > PLANT_TREE) return 0;
    if (p->garden[slot].plantType != PLANT_NONE) return 0; /* slot occupied */
    if (p->seedsAvailable <= 0) return 0;
    p->garden[slot].plantType = (unsigned char)plantType;
    p->garden[slot].stage = 0; /* seed */
    p->garden[slot].plantedSession = p->sessionCount;
    p->seedsAvailable--;
    return 1;
}

int progress_garden_count(const Progress *p) {
    if (!p) return 0;
    int c = 0;
    for (int i = 0; i < GARDEN_SLOTS; i++) {
        if (p->garden[i].plantType != PLANT_NONE) c++;
    }
    return c;
}

int progress_garden_empty_slots(const Progress *p) {
    return GARDEN_SLOTS - progress_garden_count(p);
}

int progress_garden_empty(const Progress *p) {
    return progress_garden_count(p) == 0 && (!p || p->seedsAvailable <= 0);
}

const wchar_t *plant_stage_label(int stage) {
    switch (stage) {
        case 0: return L"Seed";
        case 1: return L"Sprout";
        case 2: return L"Plant";
        case 3: return L"Bloom";
        default: return L"Seed";
    }
}

const wchar_t *plant_name(int plantType) {
    switch (plantType) {
        case PLANT_SUNFLOWER: return L"Sunflower";
        case PLANT_TULIP:     return L"Tulip";
        case PLANT_CARROT:    return L"Carrot";
        case PLANT_TREE:      return L"Tree";
        default:              return L"Empty";
    }
}

const wchar_t *plant_cue(int plantType) {
    switch (plantType) {
        case PLANT_SUNFLOWER: return L"A bright sunflower! It loves the sun.";
        case PLANT_TULIP:     return L"A pretty tulip! It has colorful petals.";
        case PLANT_CARROT:    return L"A crunchy carrot! It grows underground.";
        case PLANT_TREE:      return L"A tall tree! It gives us shade.";
        default:              return L"";
    }
}