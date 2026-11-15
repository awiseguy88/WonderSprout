#include "curriculum.h"
#include <string.h>

static const Activity letters[] = {
    {{"LT_A"}, DOMAIN_LETTERS, 1, "Letter A", "A — ah, apple"},
    {{"LT_B"}, DOMAIN_LETTERS, 1, "Letter B", "B — buh, ball"},
    {{"LT_C"}, DOMAIN_LETTERS, 1, "Letter C", "C — cuh, cat"},
    {{"LT_D"}, DOMAIN_LETTERS, 1, "Letter D", "D — duh, dog"},
    {{"LT_E"}, DOMAIN_LETTERS, 1, "Letter E", "E — eh, elephant"},
    {{"LT_FIND"}, DOMAIN_LETTERS, 3, "Find the Letter", "Tap the shown letter"},
    {{"LT_TRACE"}, DOMAIN_LETTERS, 4, "Trace a Letter", "Tap the dots along the letter"},
};

static const Activity numbers[] = {
    {{"NM_1"}, DOMAIN_NUMBERS, 1, "Number 1", "One"},
    {{"NM_2"}, DOMAIN_NUMBERS, 1, "Number 2", "Two"},
    {{"NM_3"}, DOMAIN_NUMBERS, 1, "Number 3", "Three"},
    {{"NM_4"}, DOMAIN_NUMBERS, 1, "Number 4", "Four"},
    {{"NM_5"}, DOMAIN_NUMBERS, 1, "Number 5", "Five"},
    {{"NM_COUNT"}, DOMAIN_NUMBERS, 3, "How Many?", "Count the objects"},
    {{"NM_MATCH"}, DOMAIN_NUMBERS, 3, "Match the Number", "Match number to quantity"},
};

static const Activity shapes[] = {
    {{"SC_SHAPE"}, DOMAIN_SHAPES_COLORS, 2, "Shape Match", "Match the shape"},
    {{"SC_COLOR"}, DOMAIN_SHAPES_COLORS, 2, "Color Match", "Tap the matching color circle"},
    {{"SC_SORT"}, DOMAIN_SHAPES_COLORS, 4, "Sort Shapes", "Tap all circles or all squares"},
};

static const Activity music[] = {
    {{"MU_PADS"}, DOMAIN_MUSIC, 1, "Note Pads", "Tap to play notes"},
    {{"MU_SONG1"}, DOMAIN_MUSIC, 3, "Song: Twinkle", "Repeat the melody pads"},
    {{"MU_SONG2"}, DOMAIN_MUSIC, 3, "Song: Scale", "Repeat the scale pads"},
};

const Activity k_activities[ACTIVITY_COUNT] = {
    letters[0], letters[1], letters[2], letters[3], letters[4],
    letters[5], letters[6],
    numbers[0], numbers[1], numbers[2], numbers[3], numbers[4],
    numbers[5], numbers[6],
    shapes[0], shapes[1], shapes[2],
    music[0], music[1], music[2],
};

const char * const k_learningPath[PATH_LEN] = {
    "LT_A", "NM_1", "LT_B", "SC_SHAPE",
    "LT_C", "NM_2", "SC_COLOR", "LT_D",
    "MU_PADS", "NM_3", "LT_E", "NM_4",
    "SC_SORT", "MU_SONG1", "LT_FIND", "NM_MATCH",
};
const size_t k_pathLen = PATH_LEN;

int activity_index_of(const char *id) {
    if (!id) return -1;
    for (int i = 0; i < ACTIVITY_COUNT; i++) {
        if (strcmp(k_activities[i].id, id) == 0) return i;
    }
    return -1;
}

int activity_path_position(int actIdx) {
    if (actIdx < 0 || actIdx >= ACTIVITY_COUNT) return -1;
    for (size_t i = 0; i < k_pathLen; i++) {
        int j = activity_index_of(k_learningPath[i]);
        if (j == actIdx) return (int)i;
    }
    return -1;
}

int activity_is_on_path(int actIdx) {
    return activity_path_position(actIdx) >= 0;
}

const Activity *activity_find(int actIdx) {
    if (actIdx < 0 || actIdx >= ACTIVITY_COUNT) return NULL;
    return &k_activities[actIdx];
}
