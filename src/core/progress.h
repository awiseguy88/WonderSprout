#ifndef WONDERSPROUT_PROGRESS_H
#define WONDERSPROUT_PROGRESS_H

#include "curriculum.h"

#define PROGRESS_VERSION 2

/* ---------- Garden: the persistent reason to come back ---------- */
#define GARDEN_SLOTS 6
#define PLANT_COUNT 4
#define SESSION_GOAL 3 /* activities needed to complete one play session */

typedef enum {
    PLANT_NONE = 0,
    PLANT_SUNFLOWER = 1,
    PLANT_TULIP = 2,
    PLANT_CARROT = 3,
    PLANT_TREE = 4
} PlantType;

typedef struct {
    unsigned char plantType; /* 0 = empty */
    unsigned char stage;     /* 0 = seed, 1 = sprout, 2 = plant, 3 = bloom */
    int plantedSession;      /* session number when planted */
} GardenPlant;

typedef struct {
    int version;
    unsigned char completed[ACTIVITY_COUNT];   /* 0/1 per activity */
    unsigned char stars[ACTIVITY_COUNT];       /* 0..3 stars per activity */
    int pathIndex;                             /* next step index in k_learningPath */
    unsigned char stickers[STICKER_COUNT];     /* 0/1 per sticker */
    unsigned char muted;                       /* audio off */
    unsigned char reduceMotion;                /* reduce animation */

    /* One complete play session: do SESSION_GOAL activities, then plant a seed. */
    GardenPlant garden[GARDEN_SLOTS];
    int sessionCount;      /* total completed sessions */
    int sessionActive;     /* 1 if mid-session */
    int sessionDone;       /* activities completed this session */
    int seedsAvailable;    /* un-planted seeds earned this session */
} Progress;

/* Reset p to a fresh default state. */
void progress_init(Progress *p);

/* 1 if actIdx is playable now, 0 otherwise. */
int progress_is_unlocked(const Progress *p, int actIdx);

/* Record completion of actIdx with score 0..3. Returns stars now held for it.
   Advances pathIndex if this completed the current path step. */
int progress_complete(Progress *p, int actIdx, int score);

/* actIdx of the next locked path step (-1 if path is finished). */
int progress_next_path_step(const Progress *p);

/* Sum of all stars awarded. */
int progress_star_total(const Progress *p);

/* Number of stickers currently owned. */
int progress_sticker_count(const Progress *p);

/* Bitmask of stickers that should be owned right now. */
unsigned int progress_sticker_mask(const Progress *p);

/* ---------- Garden / session API ---------- */

/* Begin a new play session (clears sessionDone). */
void progress_start_session(Progress *p);

/* 1 when the current session has reached its goal. */
int progress_session_ready(const Progress *p);

/* Finish a session: water the whole garden (advance every plant one stage),
   grant a seed, and clear the active session. */
void progress_finish_session(Progress *p);

/* Plant a seed of `plantType` into slot `slot`. Returns 1 on success. */
int progress_plant_seed(Progress *p, int slot, int plantType);

/* Number of planted (non-empty) plants. */
int progress_garden_count(const Progress *p);

/* Number of empty slots available for planting. */
int progress_garden_empty_slots(const Progress *p);

/* Stage label for a plant (seed/sprout/plant/bloom). */
const wchar_t *plant_stage_label(int stage);

/* Display name and spoken cue for a plant type. */
const wchar_t *plant_name(int plantType);
const wchar_t *plant_cue(int plantType);

/* 1 if the garden is empty (nothing planted yet). */
int progress_garden_empty(const Progress *p);

#endif