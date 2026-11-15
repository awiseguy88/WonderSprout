#ifndef WONDERSPROUT_CURRICULUM_H
#define WONDERSPROUT_CURRICULUM_H

#include <stddef.h>

#define ACTIVITY_COUNT 20
#define STICKER_COUNT 12
#define PATH_LEN 16

typedef enum {
    DOMAIN_LETTERS,
    DOMAIN_NUMBERS,
    DOMAIN_SHAPES_COLORS,
    DOMAIN_MUSIC,
    DOMAIN_COUNT
} Domain;

typedef struct {
    const char id[16];
    Domain domain;
    int difficulty;      /* 1 (easiest) .. 5 */
    const char *title;   /* on-screen title */
    const char *cue;     /* on-screen spoken-style cue label */
} Activity;

extern const Activity k_activities[ACTIVITY_COUNT];
extern const char * const k_learningPath[PATH_LEN];
extern const size_t k_pathLen;

/* Look up an activity index by id (-1 if not found). */
int activity_index_of(const char *id);
/* Position of actIdx inside k_learningPath (-1 if not on the path). */
int activity_path_position(int actIdx);
/* Is actIdx on the Learning Path? */
int activity_is_on_path(int actIdx);
/* Pointer to the activity record at actIdx. */
const Activity *activity_find(int actIdx);

#endif
