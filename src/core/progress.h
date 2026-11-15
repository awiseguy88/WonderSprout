#ifndef WONDERSPROUT_PROGRESS_H
#define WONDERSPROUT_PROGRESS_H

#include "curriculum.h"

#define PROGRESS_VERSION 1

typedef struct {
    int version;
    unsigned char completed[ACTIVITY_COUNT];   /* 0/1 per activity */
    unsigned char stars[ACTIVITY_COUNT];       /* 0..3 stars per activity */
    int pathIndex;                             /* next step index in k_learningPath */
    unsigned char stickers[STICKER_COUNT];     /* 0/1 per sticker */
    unsigned char muted;                       /* audio off */
    unsigned char reduceMotion;                /* reduce animation */
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

/* Bitmask of stickers that should be owned right now (based on stars &
   domain/path milestones). Caller diffs against p->stickers to award new ones. */
unsigned int progress_sticker_mask(const Progress *p);

#endif
