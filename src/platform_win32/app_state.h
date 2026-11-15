#ifndef APP_STATE_H
#define APP_STATE_H

#include <windows.h>
#include "core/curriculum.h"
#include "core/progress.h"

typedef enum {
    SCREEN_HOME = 0,
    SCREEN_PATH,
    SCREEN_LETTERS,
    SCREEN_NUMBERS,
    SCREEN_SHAPES,
    SCREEN_MUSIC,
    SCREEN_REWARDS,
    SCREEN_SETTINGS,
    SCREEN_ACTIVITY,
    SCREEN_COUNT
} Screen;

typedef enum {
    BTN_NONE = 0,
    BTN_HOME_PATH,
    BTN_HOME_LETTERS,
    BTN_HOME_NUMBERS,
    BTN_HOME_SHAPES,
    BTN_HOME_MUSIC,
    BTN_HOME_REWARDS,
    BTN_HOME_SETTINGS,
    BTN_BACK,
    BTN_MUTE,
    BTN_REDUCE_MOTION,
    BTN_RESET,
    BTN_CONFIRM_YES,
    BTN_CONFIRM_NO,
    BTN_NEXT_STEP,
    BTN_PLAY_SONG1,
    BTN_PLAY_SONG2,
    BTN_PAD_1, BTN_PAD_2, BTN_PAD_3, BTN_PAD_4, BTN_PAD_5, BTN_PAD_6, BTN_PAD_7, BTN_PAD_8,
    BTN_ACTIVITY_START,
    BTN_ACTIVITY_ACTION, /* generic tap in activity area */
    BTN_COUNT
} ButtonId;

typedef struct {
    RECT rc;
    ButtonId id;
    const wchar_t *label;
    int param; /* extra data (e.g. activity index) */
    int enabled;
} HitRect;

#define MAX_HITS 64

typedef struct {
    Screen screen;
    Screen prevScreen;
    Progress progress;
    char savePath[MAX_PATH];

    /* current activity */
    int activeActIdx; /* index into k_activities, -1 if none */
    int actScore;     /* stars/score for current run */
    int actDone;      /* 1 when finished */
    int actStep;      /* internal step within activity */

    /* transient UI */
    int showResetConfirm;
    int saveError;            /* 1 if the last save failed to persist */
    wchar_t statusMsg[128];

    /* hit regions for the current frame */
    HitRect hits[MAX_HITS];
    int hitCount;

    /* window */
    int width, height;
} AppState;

void app_init(AppState *app);
void app_shutdown(AppState *app);
void app_layout(AppState *app, int w, int h);

/* Handle a left-click at (x,y). Returns 1 if state changed and needs repaint. */
int app_click(AppState *app, int x, int y);

/* Draw the current screen into hdc. */
void app_render(AppState *app, HDC hdc);

/* Helpers to get progress file path. */
void app_get_save_path(char *buf, int bufLen);

#endif
