#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

#include "app_state.h"
#include "audio_winmm.h"
#include "core/storage.h"
#include "core/audio_synth.h"

/* ---------- helpers ---------- */
static void make_dir_if_needed(const char *path) {
    /* CreateDirectoryA is fine; ignore errors if exists. */
    CreateDirectoryA(path, NULL);
}

void app_get_save_path(char *buf, int bufLen) {
    char appData[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
        snprintf(buf, bufLen, "%s\\WonderSprout", appData);
        make_dir_if_needed(buf);
        snprintf(buf, bufLen, "%s\\WonderSprout\\progress.dat", appData);
    } else {
        snprintf(buf, bufLen, "wondersprout_progress.dat");
    }
}

static void set_status(AppState *app, const wchar_t *msg) {
    if (!msg) { app->statusMsg[0] = 0; return; }
    wcsncpy(app->statusMsg, msg, 127);
    app->statusMsg[127] = 0;
}

static void play_feedback(AppState *app, int good) {
    if (app->progress.muted) return;
    ToneBuffer b = good ? synth_star(44100) : synth_error(44100);
    audio_play(&b);
    tone_buffer_free(&b);
}

static void play_tone(AppState *app, float freq, float dur) {
    if (app->progress.muted) return;
    ToneBuffer b = synth_tone(freq, dur, 44100);
    audio_play(&b);
    tone_buffer_free(&b);
}

static void award_stickers(AppState *app) {
    unsigned int mask = progress_sticker_mask(&app->progress);
    for (int i = 0; i < STICKER_COUNT; i++) {
        if ((mask & (1u << i)) && !app->progress.stickers[i]) {
            app->progress.stickers[i] = 1;
        }
    }
}

static void save(AppState *app) {
    storage_save(app->savePath, &app->progress);
}

/* ---------- hit rects ---------- */
static void hits_clear(AppState *app) { app->hitCount = 0; }

static void hit_add(AppState *app, RECT rc, ButtonId id, const wchar_t *label, int param, int enabled) {
    if (app->hitCount >= MAX_HITS) return;
    HitRect *h = &app->hits[app->hitCount++];
    h->rc = rc; h->id = id; h->label = label; h->param = param; h->enabled = enabled;
}

static ButtonId hit_test(AppState *app, int x, int y, int *outParam) {
    POINT pt = {x, y};
    for (int i = app->hitCount - 1; i >= 0; i--) {
        if (!app->hits[i].enabled) continue;
        if (PtInRect(&app->hits[i].rc, pt)) {
            if (outParam) *outParam = app->hits[i].param;
            return app->hits[i].id;
        }
    }
    if (outParam) *outParam = 0;
    return BTN_NONE;
}

/* ---------- layout ---------- */
void app_layout(AppState *app, int w, int h) {
    app->width = w; app->height = h;
}

/* ---------- lifecycle ---------- */
void app_init(AppState *app) {
    memset(app, 0, sizeof(*app));
    app->screen = SCREEN_HOME;
    app->activeActIdx = -1;
    app_get_save_path(app->savePath, (int)sizeof(app->savePath));
    if (storage_load(app->savePath, &app->progress) != 0) {
        progress_init(&app->progress);
    }
    audio_init();
    set_status(app, L"Welcome! Tap a big button to play.");
}

void app_shutdown(AppState *app) {
    save(app);
    audio_shutdown();
}

/* ---------- activity logic ---------- */
static void start_activity(AppState *app, int actIdx) {
    if (actIdx < 0 || actIdx >= ACTIVITY_COUNT) return;
    app->activeActIdx = actIdx;
    app->actScore = 0;
    app->actDone = 0;
    app->actStep = 0;
    app->screen = SCREEN_ACTIVITY;
    set_status(app, L"Let's play!");
}

static void finish_activity(AppState *app) {
    if (app->activeActIdx < 0) return;
    int stars = app->actScore;
    if (stars < 1) stars = 1; /* participation star */
    progress_complete(&app->progress, app->activeActIdx, stars);
    award_stickers(app);
    save(app);
    app->actDone = 1;
    set_status(app, L"Great job! You earned a star!");
    play_feedback(app, 1);
}

/* ---------- drawing helpers ---------- */
static void fill_rect(HDC hdc, RECT rc, COLORREF c) {
    HBRUSH br = CreateSolidBrush(c);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
}

static void draw_text_center(HDC hdc, RECT rc, const wchar_t *text, int size, COLORREF color, int bold) {
    HFONT f = CreateFontW(-size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT old = (HFONT)SelectObject(hdc, f);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, old);
    DeleteObject(f);
}

static void draw_text_wrap(HDC hdc, RECT rc, const wchar_t *text, int size, COLORREF color) {
    HFONT f = CreateFontW(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT old = (HFONT)SelectObject(hdc, f);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(hdc, old);
    DeleteObject(f);
}

static void draw_button(AppState *app, HDC hdc, RECT rc, ButtonId id, const wchar_t *label, int param, COLORREF bg, int enabled) {
    COLORREF fill = enabled ? bg : RGB(180,180,180);
    fill_rect(hdc, rc, fill);
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(40,40,40));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
    draw_text_center(hdc, rc, label, 22, RGB(20,20,20), 1);
    hit_add(app, rc, id, label, param, enabled);
}

static void draw_back(AppState *app, HDC hdc) {
    RECT rc = {10, 10, 120, 50};
    draw_button(app, hdc, rc, BTN_BACK, L"< Back", 0, RGB(220,220,220), 1);
}

/* ---------- render screens ---------- */
static void render_home(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0,0,app->width,app->height}, RGB(255, 250, 235));
    draw_text_center(hdc, (RECT){0, 20, app->width, 80}, L"WonderSprout", 42, RGB(40,120,60), 1);
    draw_text_center(hdc, (RECT){0, 75, app->width, 110}, L"Free learning fun for ages 2-5", 20, RGB(80,80,80), 0);

    int cols = 3, rows = 2;
    int pad = 20, top = 130, bw = (app->width - pad*(cols+1)) / cols;
    int bh = (app->height - top - pad*(rows+1)) / rows;
    struct { const wchar_t *label; ButtonId id; COLORREF c; } btns[6] = {
        {L"Learning Path", BTN_HOME_PATH, RGB(255,220,120)},
        {L"Letters", BTN_HOME_LETTERS, RGB(180,220,255)},
        {L"Numbers", BTN_HOME_NUMBERS, RGB(255,200,200)},
        {L"Shapes & Colors", BTN_HOME_SHAPES, RGB(200,255,200)},
        {L"Music", BTN_HOME_MUSIC, RGB(220,200,255)},
        {L"My Rewards", BTN_HOME_REWARDS, RGB(255,230,180)},
    };
    for (int i = 0; i < 6; i++) {
        int r = i / cols, c = i % cols;
        RECT rc = { pad + c*(bw+pad), top + pad + r*(bh+pad), pad + c*(bw+pad) + bw, top + pad + r*(bh+pad) + bh };
        draw_button(app, hdc, rc, btns[i].id, btns[i].label, 0, btns[i].c, 1);
    }
    /* settings in corner */
    RECT s = {app->width - 140, 10, app->width - 10, 50};
    draw_button(app, hdc, s, BTN_HOME_SETTINGS, L"Settings", 0, RGB(230,230,230), 1);

    draw_text_center(hdc, (RECT){0, app->height-40, app->width, app->height-5}, app->statusMsg, 18, RGB(60,60,60), 0);
}

static void render_path(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0,0,app->width,app->height}, RGB(255,245,210));
    draw_back(app, hdc);
    draw_text_center(hdc, (RECT){130, 10, app->width-10, 55}, L"Learning Path", 32, RGB(60,60,60), 1);

    int y = 70, rowH = 38;
    for (int i = 0; i < (int)k_pathLen; i++) {
        int idx = activity_index_of(k_learningPath[i]);
        if (idx < 0) continue;
        int unlocked = progress_is_unlocked(&app->progress, idx);
        int done = app->progress.completed[idx];
        wchar_t line[128];
        const char *title = k_activities[idx].title;
        wchar_t wtitle[64];
        MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, 64);
        swprintf(line, 128, L"%d. %ls %ls", i+1, wtitle, done ? L"[STAR]" : (unlocked ? L"(ready)" : L"(locked)"));
        RECT rc = {40, y, app->width - 160, y + rowH - 4};
        COLORREF bg = done ? RGB(200,255,200) : (unlocked ? RGB(255,240,180) : RGB(220,220,220));
        fill_rect(hdc, rc, bg);
        draw_text_center(hdc, rc, line, 18, RGB(30,30,30), 0);
        RECT play = {app->width - 150, y, app->width - 30, y + rowH - 4};
        if (unlocked) {
            /* reuse BTN_ACTIVITY_START with param = idx by adding a dynamic hit */
            draw_button(app, hdc, play, BTN_ACTIVITY_START, L"Play", idx, RGB(150,230,150), 1);
        } else {
            draw_button(app, hdc, play, BTN_NONE, L"Locked", 0, RGB(200,200,200), 0);
        }
        y += rowH;
        if (y > app->height - 60) break;
    }
}

static void render_activity_list(AppState *app, HDC hdc, Domain dom, const wchar_t *title, COLORREF bgc) {
    fill_rect(hdc, (RECT){0,0,app->width,app->height}, bgc);
    draw_back(app, hdc);
    draw_text_center(hdc, (RECT){130, 10, app->width-10, 55}, title, 32, RGB(40,40,40), 1);
    int y = 80;
    for (int i = 0; i < ACTIVITY_COUNT; i++) {
        if (k_activities[i].domain != dom) continue;
        wchar_t wtitle[64];
        MultiByteToWideChar(CP_UTF8, 0, k_activities[i].title, -1, wtitle, 64);
        wchar_t line[96];
        swprintf(line, 96, L"%ls %ls", wtitle, app->progress.completed[i] ? L"[STAR]" : L"");
        RECT rc = {60, y, app->width - 220, y + 50};
        fill_rect(hdc, rc, RGB(255,255,255));
        draw_text_center(hdc, rc, line, 20, RGB(30,30,30), 0);
        RECT play = {app->width - 200, y, app->width - 60, y + 50};
        draw_button(app, hdc, play, BTN_ACTIVITY_START, L"Play", i, RGB(170,230,170), 1);
        y += 60;
        if (y > app->height - 40) break;
    }
}

static void render_rewards(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0,0,app->width,app->height}, RGB(255,248,220));
    draw_back(app, hdc);
    draw_text_center(hdc, (RECT){130,10,app->width-10,55}, L"My Rewards", 32, RGB(60,60,60), 1);
    wchar_t buf[128];
    swprintf(buf, 128, L"Stars: %d    Stickers: %d", progress_star_total(&app->progress), progress_sticker_count(&app->progress));
    draw_text_center(hdc, (RECT){0,70,app->width,110}, buf, 26, RGB(40,100,40), 1);

    int x = 60, y = 130, size = 90, gap = 20, col = 0;
    for (int i = 0; i < STICKER_COUNT; i++) {
        RECT rc = {x, y, x+size, y+size};
        COLORREF c = app->progress.stickers[i] ? RGB(255,215,0) : RGB(220,220,220);
        fill_rect(hdc, rc, c);
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(80,80,80));
        HGDIOBJ op = SelectObject(hdc, pen); HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Ellipse(hdc, rc.left+5, rc.top+5, rc.right-5, rc.bottom-5);
        SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pen);
        wchar_t lab[8]; swprintf(lab, 8, L"%d", i+1);
        draw_text_center(hdc, rc, lab, 20, RGB(40,40,40), 1);
        x += size + gap; col++;
        if (col >= 6) { col = 0; x = 60; y += size + gap; }
    }
}

static void render_settings(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0,0,app->width,app->height}, RGB(240,240,245));
    draw_back(app, hdc);
    draw_text_center(hdc, (RECT){130,10,app->width-10,55}, L"Settings (Parents)", 30, RGB(50,50,50), 1);

    RECT mute = {80, 100, 420, 160};
    draw_button(app, hdc, mute, BTN_MUTE, app->progress.muted ? L"Sound: OFF (tap)" : L"Sound: ON (tap)", 0, RGB(200,230,255), 1);
    RECT rm = {80, 180, 420, 240};
    draw_button(app, hdc, rm, BTN_REDUCE_MOTION, app->progress.reduceMotion ? L"Reduce Motion: ON" : L"Reduce Motion: OFF", 0, RGB(200,230,255), 1);
    RECT reset = {80, 280, 420, 340};
    draw_button(app, hdc, reset, BTN_RESET, L"Reset Progress", 0, RGB(255,200,200), 1);

    if (app->showResetConfirm) {
        RECT box = {460, 240, 760, 380};
        fill_rect(hdc, box, RGB(255,255,220));
        draw_text_wrap(hdc, (RECT){470,250,750,300}, L"Reset all stars and progress?", 20, RGB(30,30,30));
        draw_button(app, hdc, (RECT){480,310,590,360}, BTN_CONFIRM_YES, L"Yes", 0, RGB(255,180,180), 1);
        draw_button(app, hdc, (RECT){610,310,720,360}, BTN_CONFIRM_NO, L"No", 0, RGB(200,255,200), 1);
    }

    draw_text_wrap(hdc, (RECT){80, 380, app->width-80, app->height-20},
        L"WonderSprout is 100% free and offline. No accounts, no ads, no tracking, no network. Progress is saved locally on this device only.",
        18, RGB(70,70,70));
}

/* ---------- activity rendering & interaction helpers ---------- */
static void render_activity_header(AppState *app, HDC hdc) {
    draw_back(app, hdc);
    if (app->activeActIdx < 0) return;
    const Activity *a = &k_activities[app->activeActIdx];
    wchar_t title[128], cue[256];
    MultiByteToWideChar(CP_UTF8, 0, a->title, -1, title, 128);
    MultiByteToWideChar(CP_UTF8, 0, a->cue ? a->cue : "", -1, cue, 256);
    draw_text_center(hdc, (RECT){130, 5, app->width-10, 45}, title, 28, RGB(30,30,30), 1);
    draw_text_center(hdc, (RECT){130, 42, app->width-10, 75}, cue, 20, RGB(80,80,80), 0);
}

static void render_activity_done(AppState *app, HDC hdc) {
    RECT box = {app->width/2 - 200, app->height/2 - 80, app->width/2 + 200, app->height/2 + 100};
    fill_rect(hdc, box, RGB(255, 245, 180));
    draw_text_center(hdc, (RECT){box.left, box.top+10, box.right, box.top+60}, L"Great job! Star earned!", 26, RGB(40,120,40), 1);
    draw_button(app, hdc, (RECT){box.left+40, box.top+70, box.right-40, box.top+130}, BTN_BACK, L"Back", 0, RGB(180,240,180), 1);
}

/* We implement activities as simple interactive screens driven by activeActIdx/actStep.
   For brevity and robustness we use tap-to-complete/choice mechanics that are fully
   playable with a mouse: show prompt + big choices; correct tap advances/completes. */

static void render_letter_gallery(AppState *app, HDC hdc, int letterIndex) {
    /* letterIndex 0..4 for A..E */
    wchar_t letter[4] = {L'A' + (wchar_t)letterIndex, 0};
    const wchar_t *words[5] = {L"A - Apple", L"B - Ball", L"C - Cat", L"D - Dog", L"E - Elephant"};
    fill_rect(hdc, (RECT){0,0,app->width,app->height}, RGB(220,240,255));
    render_activity_header(app, hdc);
    draw_text_center(hdc, (RECT){0, 120, app->width, 320}, letter, 160, RGB(40,80,160), 1);
    draw_text_center(hdc, (RECT){0, 320, app->width, 380}, words[letterIndex], 36, RGB(50,50,50), 1);
    draw_button(app, hdc, (RECT){app->width/2 - 120, 420, app->width/2 + 120, 490}, BTN_ACTIVITY_ACTION, L"I know it!", 1, RGB(180,255,180), 1);
    if (app->actDone) render_activity_done(app, hdc);
}

static void render_find_letter(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0,0,app->width,app->height}, RGB(230,245,255));
    render_activity_header(app, hdc);
    /* target cycles by step */
    wchar_t targets[5] = {L'A',L'B',L'C',L'D',L'E'};
    wchar_t target = targets[app->actStep % 5];
    wchar_t prompt[64]; swprintf(prompt, 64, L"Tap the letter: %lc", target);
    draw_text_center(hdc, (RECT){0, 90, app->width, 140}, prompt, 32, RGB(30,30,30), 1);
    /* 4 choices laid out; correct one is at position (actStep % 4) */
    int correctPos = app->actStep % 4;
    wchar_t opts[4];
    for (int i = 0; i < 4; i++) opts[i] = L'A' + (wchar_t)((app->actStep + i*2) % 5);
    opts[correctPos] = target;
    int bw = 140, bh = 120, gap = 30, total = 4*bw + 3*gap;
    int startX = (app->width - total) / 2, y = 220;
    for (int i = 0; i < 4; i++) {
        RECT rc = {startX + i*(bw+gap), y, startX + i*(bw+gap) + bw, y + bh};
        wchar_t lab[2] = {opts[i], 0};
        int isCorrect = (i == correctPos);
        draw_button(app, hdc, rc, BTN_ACTIVITY_ACTION, lab, isCorrect ? 1 : 0, RGB(255,255,200), 1);
    }
    if (app->actDone) render_activity_done(app, hdc);
}

static void render_number_show(AppState *app, HDC hdc, int n) {
    fill_rect(hdc, (RECT){0,0,app->width,app->height}, RGB(255,235,235));
    render_activity_header(app, hdc);
    wchar_t num[8]; swprintf(num, 8, L"%d", n);
    draw_text_center(hdc, (RECT){0, 90, app->width/2, 200}, num, 120, RGB(160,40,40), 1);
    /* draw n circles */
    int startX = app->width/2, cy = 180, r = 22, gap = 12;
    for (int i = 0; i < n && i < 10; i++) {
        int cx = startX + 40 + (i % 5) * (2*r + gap);
        int yy = cy + (i / 5) * (2*r + gap);
        HBRUSH br = CreateSolidBrush(RGB(255,120,120));
        HGDIOBJ ob = SelectObject(hdc, br);
        Ellipse(hdc, cx-r, yy-r, cx+r, yy+r);
        SelectObject(hdc, ob); DeleteObject(br);
    }
    draw_button(app, hdc, (RECT){app->width/2 - 120, 380, app->width/2 + 120, 450}, BTN_ACTIVITY_ACTION, L"I counted!", 1, RGB(180,255,180), 1);
    if (app->actDone) render_activity_done(app, hdc);
}

static void render_count_quiz(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0,0,app->width,app->height}, RGB(255,240,240));
    render_activity_header(app, hdc);
    int counts[5] = {2,3,4,5,3};
    int target = counts[app->actStep % 5];
    draw_text_center(hdc, (RECT){0, 90, app->width, 130}, L"How many circles?", 28, RGB(30,30,30), 1);
    /* draw target circles */
    int cx0 = app->width/2 - (target*30);
    for (int i = 0; i < target; i++) {
        int cx = cx0 + i*60 + 30, cy = 190;
        HBRUSH br = CreateSolidBrush(RGB(100,150,255));
        HGDIOBJ ob = SelectObject(hdc, br);
        Ellipse(hdc, cx-22, cy-22, cx+22, cy+22);
        SelectObject(hdc, ob); DeleteObject(br);
    }
    int opts[4] = {target-1, target, target+1, target+2};
    for (int i=0;i<4;i++) if (opts[i] < 1) opts[i] = target + i + 1;
    /* ensure one correct */
    opts[app->actStep % 4] = target;
    int bw=100,bh=90,gap=25,total=4*bw+3*gap, sx=(app->width-total)/2, y=300;
    for (int i=0;i<4;i++) {
        wchar_t lab[4]; swprintf(lab,4,L"%d",opts[i]);
        draw_button(app, hdc, (RECT){sx+i*(bw+gap), y, sx+i*(bw+gap)+bw, y+bh}, BTN_ACTIVITY_ACTION, lab, opts[i], RGB(255,255,210), 1);
        /* store correct via param: param==target means correct */
    }
    if (app->actDone) render_activity_done(app, hdc);
}

static void render_shape_match(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0,0,app->width,app->height}, RGB(235,255,235));
    render_activity_header(app, hdc);
    const wchar_t *names[4] = {L"Circle", L"Square", L"Triangle", L"Star"};
    int target = app->actStep % 4;
    wchar_t prompt[64]; swprintf(prompt,64,L"Tap the %ls", names[target]);
    draw_text_center(hdc, (RECT){0,90,app->width,130}, prompt, 30, RGB(30,30,30), 1);
    int bw=140,bh=140,gap=30,total=4*bw+3*gap,sx=(app->width-total)/2,y=200;
    for (int i=0;i<4;i++) {
        RECT rc = {sx+i*(bw+gap), y, sx+i*(bw+gap)+bw, y+bh};
        fill_rect(hdc, rc, RGB(255,255,240));
        HPEN pen = CreatePen(PS_SOLID,3,RGB(40,120,40));
        HBRUSH br = CreateSolidBrush(RGB(180,240,180));
        HGDIOBJ op=SelectObject(hdc,pen), ob=SelectObject(hdc,br);
        int cx=(rc.left+rc.right)/2, cy=(rc.top+rc.bottom)/2;
        if (i==0) Ellipse(hdc,cx-40,cy-40,cx+40,cy+40);
        else if (i==1) Rectangle(hdc,cx-40,cy-40,cx+40,cy+40);
        else if (i==2) { POINT pts[3]={{cx,cy-45},{cx-45,cy+35},{cx+45,cy+35}}; Polygon(hdc,pts,3); }
        else { /* star simplified as diamond */ POINT pts[4]={{cx,cy-45},{cx+40,cy},{cx,cy+45},{cx-40,cy}}; Polygon(hdc,pts,4); }
        SelectObject(hdc,op); SelectObject(hdc,ob); DeleteObject(pen); DeleteObject(br);
        hit_add(app, rc, BTN_ACTIVITY_ACTION, names[i], (i==target)?1:0, 1);
    }
    if (app->actDone) render_activity_done(app, hdc);
}

static void render_music_pads(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0,0,app->width,app->height}, RGB(245,235,255));
    render_activity_header(app, hdc);
    const wchar_t *labs[8] = {L"C",L"D",L"E",L"F",L"G",L"A",L"B",L"C2"};
    COLORREF cols[8] = {RGB(255,150,150),RGB(255,200,150),RGB(255,255,150),RGB(180,255,180),RGB(150,220,255),RGB(180,180,255),RGB(220,150,255),RGB(255,150,220)};
    int bw=100,bh=160,gap=15,total=8*bw+7*gap,sx=(app->width-total)/2,y=180;
    for (int i=0;i<8;i++) {
        RECT rc={sx+i*(bw+gap),y,sx+i*(bw+gap)+bw,y+bh};
        draw_button(app, hdc, rc, (ButtonId)(BTN_PAD_1+i), labs[i], i, cols[i], 1);
    }
    draw_button(app, hdc, (RECT){app->width/2-100, 380, app->width/2+100, 440}, BTN_ACTIVITY_ACTION, L"Done playing", 1, RGB(200,255,200), 1);
    if (app->actDone) render_activity_done(app, hdc);
}

static void render_activity(AppState *app, HDC hdc) {
    if (app->activeActIdx < 0) { render_home(app, hdc); return; }
    const char *id = k_activities[app->activeActIdx].id;
    if (strcmp(id,"LT_A")==0) render_letter_gallery(app,hdc,0);
    else if (strcmp(id,"LT_B")==0) render_letter_gallery(app,hdc,1);
    else if (strcmp(id,"LT_C")==0) render_letter_gallery(app,hdc,2);
    else if (strcmp(id,"LT_D")==0) render_letter_gallery(app,hdc,3);
    else if (strcmp(id,"LT_E")==0) render_letter_gallery(app,hdc,4);
    else if (strcmp(id,"LT_FIND")==0 || strcmp(id,"LT_TRACE")==0) render_find_letter(app,hdc);
    else if (strcmp(id,"NM_1")==0) render_number_show(app,hdc,1);
    else if (strcmp(id,"NM_2")==0) render_number_show(app,hdc,2);
    else if (strcmp(id,"NM_3")==0) render_number_show(app,hdc,3);
    else if (strcmp(id,"NM_4")==0) render_number_show(app,hdc,4);
    else if (strcmp(id,"NM_5")==0) render_number_show(app,hdc,5);
    else if (strcmp(id,"NM_COUNT")==0 || strcmp(id,"NM_MATCH")==0) render_count_quiz(app,hdc);
    else if (strcmp(id,"SC_SHAPE")==0 || strcmp(id,"SC_COLOR")==0 || strcmp(id,"SC_SORT")==0) render_shape_match(app,hdc);
    else if (strcmp(id,"MU_PADS")==0 || strcmp(id,"MU_SONG1")==0 || strcmp(id,"MU_SONG2")==0) render_music_pads(app,hdc);
    else { fill_rect(hdc,(RECT){0,0,app->width,app->height},RGB(240,240,240)); render_activity_header(app,hdc); draw_button(app,hdc,(RECT){app->width/2-100,300,app->width/2+100,360},BTN_ACTIVITY_ACTION,L"Finish",1,RGB(200,255,200),1); if(app->actDone) render_activity_done(app,hdc); }
}

void app_render(AppState *app, HDC hdc) {
    hits_clear(app);
    switch (app->screen) {
        case SCREEN_HOME: render_home(app, hdc); break;
        case SCREEN_PATH: render_path(app, hdc); break;
        case SCREEN_LETTERS: render_activity_list(app, hdc, DOMAIN_LETTERS, L"Letters", RGB(220,240,255)); break;
        case SCREEN_NUMBERS: render_activity_list(app, hdc, DOMAIN_NUMBERS, L"Numbers", RGB(255,235,235)); break;
        case SCREEN_SHAPES:  render_activity_list(app, hdc, DOMAIN_SHAPES_COLORS, L"Shapes & Colors", RGB(235,255,235)); break;
        case SCREEN_MUSIC:   render_activity_list(app, hdc, DOMAIN_MUSIC, L"Music", RGB(245,235,255)); break;
        case SCREEN_REWARDS: render_rewards(app, hdc); break;
        case SCREEN_SETTINGS:render_settings(app, hdc); break;
        case SCREEN_ACTIVITY:render_activity(app, hdc); break;
        default: render_home(app, hdc); break;
    }
}

/* ---------- click handling ---------- */
static void handle_activity_action(AppState *app, int param) {
    if (app->activeActIdx < 0) return;
    const char *id = k_activities[app->activeActIdx].id;

    /* Activities with a simple "I did it / done" action */
    if (strcmp(id,"LT_A")==0||strcmp(id,"LT_B")==0||strcmp(id,"LT_C")==0||strcmp(id,"LT_D")==0||strcmp(id,"LT_E")==0||
        strcmp(id,"NM_1")==0||strcmp(id,"NM_2")==0||strcmp(id,"NM_3")==0||strcmp(id,"NM_4")==0||strcmp(id,"NM_5")==0) {
        app->actScore = 3;
        finish_activity(app);
        return;
    }

    if (strcmp(id,"LT_FIND")==0 || strcmp(id,"LT_TRACE")==0) {
        if (param == 1) { /* correct choice */
            app->actStep++;
            play_tone(app, 660.0f, 0.12f);
            if (app->actStep >= 3) { app->actScore = 3; finish_activity(app); }
            else set_status(app, L"Nice! Find the next one.");
        } else {
            play_feedback(app, 0);
            set_status(app, L"Try again!");
        }
        return;
    }

    if (strcmp(id,"NM_COUNT")==0 || strcmp(id,"NM_MATCH")==0) {
        /* param carries the chosen number; correct if equals target count for step */
        int counts[5] = {2,3,4,5,3};
        int target = counts[app->actStep % 5];
        if (param == target) {
            app->actStep++;
            play_tone(app, 700.0f, 0.12f);
            if (app->actStep >= 3) { app->actScore = 3; finish_activity(app); }
            else set_status(app, L"Correct! How many now?");
        } else {
            play_feedback(app, 0);
            set_status(app, L"Try again - count carefully!");
        }
        return;
    }

    if (strcmp(id,"SC_SHAPE")==0 || strcmp(id,"SC_COLOR")==0 || strcmp(id,"SC_SORT")==0) {
        if (param == 1) {
            app->actStep++;
            play_tone(app, 720.0f, 0.12f);
            if (app->actStep >= 3) { app->actScore = 3; finish_activity(app); }
            else set_status(app, L"Great! Match the next one.");
        } else {
            play_feedback(app, 0);
            set_status(app, L"Try a different one!");
        }
        return;
    }

    if (strcmp(id,"MU_PADS")==0 || strcmp(id,"MU_SONG1")==0 || strcmp(id,"MU_SONG2")==0) {
        /* "Done playing" */
        app->actScore = 2;
        finish_activity(app);
        return;
    }

    /* default */
    app->actScore = 2;
    finish_activity(app);
}

int app_click(AppState *app, int x, int y) {
    int param = 0;
    ButtonId id = hit_test(app, x, y, &param);

    if (id == BTN_NONE) return 0;

    switch (id) {
        case BTN_HOME_PATH: app->screen = SCREEN_PATH; set_status(app, L"Follow the path!"); return 1;
        case BTN_HOME_LETTERS: app->screen = SCREEN_LETTERS; return 1;
        case BTN_HOME_NUMBERS: app->screen = SCREEN_NUMBERS; return 1;
        case BTN_HOME_SHAPES:  app->screen = SCREEN_SHAPES; return 1;
        case BTN_HOME_MUSIC:   app->screen = SCREEN_MUSIC; return 1;
        case BTN_HOME_REWARDS: app->screen = SCREEN_REWARDS; return 1;
        case BTN_HOME_SETTINGS:app->screen = SCREEN_SETTINGS; app->showResetConfirm = 0; return 1;

        case BTN_BACK:
            if (app->screen == SCREEN_ACTIVITY) { app->activeActIdx = -1; app->screen = SCREEN_HOME; }
            else app->screen = SCREEN_HOME;
            app->showResetConfirm = 0;
            set_status(app, L"Welcome back!");
            return 1;

        case BTN_MUTE:
            app->progress.muted = !app->progress.muted;
            save(app);
            return 1;

        case BTN_REDUCE_MOTION:
            app->progress.reduceMotion = !app->progress.reduceMotion;
            save(app);
            return 1;

        case BTN_RESET:
            app->showResetConfirm = 1;
            return 1;

        case BTN_CONFIRM_YES:
            storage_reset(app->savePath);
            storage_load(app->savePath, &app->progress);
            app->showResetConfirm = 0;
            set_status(app, L"Progress reset.");
            return 1;

        case BTN_CONFIRM_NO:
            app->showResetConfirm = 0;
            return 1;

        case BTN_ACTIVITY_START:
            if (param >= 0 && param < ACTIVITY_COUNT) start_activity(app, param);
            return 1;

        case BTN_PAD_1: case BTN_PAD_2: case BTN_PAD_3: case BTN_PAD_4:
        case BTN_PAD_5: case BTN_PAD_6: case BTN_PAD_7: case BTN_PAD_8: {
            float freqs[8] = {261.63f,293.66f,329.63f,349.23f,392.00f,440.00f,493.88f,523.25f};
            int idx = (int)id - (int)BTN_PAD_1;
            if (idx >= 0 && idx < 8) {
                /* mark some interaction so activity can be "completed" via Done */
                play_tone(app, freqs[idx], 0.25f);
            }
            return 1;
        }

        case BTN_ACTIVITY_ACTION:
            if (app->actDone) { app->activeActIdx = -1; app->screen = SCREEN_HOME; set_status(app, L"Welcome back!"); return 1; }
            handle_activity_action(app, param);
            return 1;

        default:
            return 0;
    }
}
