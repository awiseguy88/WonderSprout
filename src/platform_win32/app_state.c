#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
#include <time.h>

#include "app_state.h"
#include "audio_winmm.h"
#include "tts_sapi.h"
#include "core/storage.h"
#include "core/audio_synth.h"

/* ---------- constants ---------- */
#define MAX_PARTICLES 40
#define PARTICLE_LIFE 60

#define BGC_LIGHT   RGB(255, 250, 235)
#define BGC_LETTERS RGB(235, 245, 255)
#define BGC_NUMBERS RGB(255, 245, 245)
#define BGC_SHAPES  RGB(240, 255, 240)
#define BGC_MUSIC   RGB(250, 240, 255)

/* Forward declarations for static helpers */
static void render_activity_header(AppState *app, HDC hdc);
static void render_activity_done(AppState *app, HDC hdc);

#define PART_MAX 16

/* ---------- particle system ---------- */
typedef struct {
    float x, y;
    float vx, vy;
    float life;
    COLORREF color;
} Particle;

static Particle g_particles[MAX_PARTICLES];
static int g_particleCount = 0;

static void particle_spawn(float x, float y, COLORREF c) {
    if (g_particleCount >= MAX_PARTICLES) return;
    Particle *p = &g_particles[g_particleCount++];
    p->x = x; p->y = y;
    float ang = ((float)rand() / RAND_MAX) * 6.28318f;
    float spd = 2.0f + ((float)rand() / RAND_MAX) * 6.0f;
    p->vx = cosf(ang) * spd;
    p->vy = sinf(ang) * spd;
    p->life = PART_MAX;
    p->color = c;
}

static void particle_update(void) {
    int active = 0;
    for (int i = 0; i < g_particleCount; i++) {
        Particle *p = &g_particles[i];
        if (p->life <= 0) continue;
        p->x += p->vx;
        p->y += p->vy;
        p->vy += 0.15f; /* gravity */
        p->life -= 1.0f;
        active++;
    }
    /* compact */
    int dst = 0;
    for (int i = 0; i < g_particleCount; i++) {
        if (g_particles[i].life > 0) {
            g_particles[dst++] = g_particles[i];
        }
    }
    g_particleCount = dst;
}

static void particle_draw(HDC hdc) {
    for (int i = 0; i < g_particleCount; i++) {
        Particle *p = &g_particles[i];
        if (p->life <= 0) continue;
        int alpha = (int)(255.0f * (p->life / (float)PART_MAX));
        if (alpha < 0) alpha = 0;
        if (alpha > 255) alpha = 255;
        COLORREF c = p->color;
        int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
        /* fade by scaling towards white */
        r = r + (255 - r) * (PART_MAX - (int)p->life) / PART_MAX;
        g = g + (255 - g) * (PART_MAX - (int)p->life) / PART_MAX;
        b = b + (255 - b) * (PART_MAX - (int)p->life) / PART_MAX;
        HBRUSH br = CreateSolidBrush(RGB(r, g, b));
        HGDIOBJ ob = SelectObject(hdc, br);
        int sz = (int)(4.0f * (p->life / (float)PART_MAX)) + 1;
        Ellipse(hdc, (int)p->x - sz, (int)p->y - sz, (int)p->x + sz, (int)p->y + sz);
        SelectObject(hdc, ob); DeleteObject(br);
    }
}

/* ---------- helpers ---------- */
static void make_dir_if_needed(const char *path) {
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

static void speak(AppState *app, const wchar_t *text) {
    if (!text || app->progress.muted) return;
    if (tts_available()) {
        tts_speak(text, true); /* async so UI stays responsive */
    }
}

static void play_feedback(AppState *app, int good) {
    if (app->progress.muted) return;
    ToneBuffer b = good ? synth_star(44100) : synth_error(44100);
    audio_play(&b);
    tone_buffer_free(&b);
    if (good) {
        /* spawn confetti */
        for (int i = 0; i < 12; i++) {
            particle_spawn(app->width / 2.0f, app->height / 2.0f,
                RGB(255, 215, 0));
            particle_spawn(app->width / 2.0f, app->height / 2.0f,
                RGB(0, 200, 255));
            particle_spawn(app->width / 2.0f, app->height / 2.0f,
                RGB(255, 80, 80));
        }
    }
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
    if (storage_save(app->savePath, &app->progress) == 0) {
        app->saveError = 0;
    } else {
        app->saveError = 1;
    }
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
    tts_init();
    srand((unsigned int)time(NULL));
    set_status(app, L"Welcome to WonderSprout! Tap a big button to play.");
    speak(app, L"Welcome to Wonder Sprout! Tap a big button to start learning.");
}

void app_shutdown(AppState *app) {
    save(app);
    tts_shutdown();
    audio_shutdown();
}

/* ---------- activity core ---------- */
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
    if (stars < 1) stars = 1;
    progress_complete(&app->progress, app->activeActIdx, stars);
    award_stickers(app);
    save(app);
    app->actDone = 1;
    set_status(app, L"Great job! Star earned!");
    play_feedback(app, 1);
    wchar_t msg[96];
    swprintf(msg, 96, L"Great job! You earned %d stars!", stars);
    speak(app, msg);
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
    SelectObject(hdc, old); DeleteObject(f);
}

static void draw_text_wrap(HDC hdc, RECT rc, const wchar_t *text, int size, COLORREF color) {
    HFONT f = CreateFontW(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT old = (HFONT)SelectObject(hdc, f);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(hdc, old); DeleteObject(f);
}

static void draw_button(AppState *app, HDC hdc, RECT rc, ButtonId id, const wchar_t *label, int param, COLORREF bg, int enabled) {
    COLORREF fill = enabled ? bg : RGB(180, 180, 180);
    fill_rect(hdc, rc, fill);
    /* inner highlight */
    RECT inner = rc; inner.top += 4; inner.left += 4; inner.right -= 4; inner.bottom -= 4;
    HBRUSH shine = CreateSolidBrush(RGB(255, 255, 255));
    FrameRect(hdc, &inner, shine);
    DeleteObject(shine);
    HPEN pen = CreatePen(PS_SOLID, 3, RGB(50, 50, 50));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 24, 24);
    SelectObject(hdc, oldPen); SelectObject(hdc, oldBr); DeleteObject(pen);
    draw_text_center(hdc, rc, label, 24, RGB(20, 20, 20), 1);
    hit_add(app, rc, id, label, param, enabled);
}

static void draw_back(AppState *app, HDC hdc) {
    RECT rc = {14, 14, 140, 62};
    draw_button(app, hdc, rc, BTN_BACK, L"< Back", 0, RGB(230, 230, 235), 1);
}

/* ---------- screens: home ---------- */
static void render_home(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0, 0, app->width, app->height}, BGC_LIGHT);
    draw_text_center(hdc, (RECT){0, 18, app->width, 86}, L"WonderSprout", 52, RGB(40, 130, 60), 1);
    draw_text_center(hdc, (RECT){0, 80, app->width, 116}, L"Free learning fun for ages 2-5", 22, RGB(80, 80, 80), 0);

    int cols = 3, rows = 2;
    int pad = 24, top = 140;
    int bw = (app->width - pad * (cols + 1)) / cols;
    int bh = (app->height - top - pad * (rows + 1)) / rows;
    struct { const wchar_t *label; ButtonId id; COLORREF c; } btns[6] = {
        {L"Learning Path", BTN_HOME_PATH, RGB(255, 220, 120)},
        {L"ABC Letters", BTN_HOME_LETTERS, RGB(180, 220, 255)},
        {L"123 Numbers", BTN_HOME_NUMBERS, RGB(255, 200, 200)},
        {L"Shapes & Colors", BTN_HOME_SHAPES, RGB(200, 255, 200)},
        {L"Music", BTN_HOME_MUSIC, RGB(220, 200, 255)},
        {L"My Rewards", BTN_HOME_REWARDS, RGB(255, 230, 180)},
    };
    for (int i = 0; i < 6; i++) {
        int r = i / cols, c = i % cols;
        RECT rc = {pad + c * (bw + pad), top + pad + r * (bh + pad),
                   pad + c * (bw + pad) + bw, top + pad + r * (bh + pad) + bh};
        draw_button(app, hdc, rc, btns[i].id, btns[i].label, 0, btns[i].c, 1);
    }
    RECT s = {app->width - 150, 14, app->width - 14, 62};
    draw_button(app, hdc, s, BTN_HOME_SETTINGS, L"\u2699 Settings", 0, RGB(230, 230, 235), 1);
    draw_text_center(hdc, (RECT){0, app->height - 34, app->width, app->height - 6}, app->statusMsg, 18, RGB(60, 60, 60), 0);
}

/* ---------- screens: learning path ---------- */
static void render_path(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0, 0, app->width, app->height}, RGB(255, 245, 210));
    draw_back(app, hdc);
    draw_text_center(hdc, (RECT){140, 10, app->width - 10, 60}, L"Learning Path", 36, RGB(80, 60, 20), 1);

    int y = 80, rowH = 52;
    for (int i = 0; i < (int)k_pathLen; i++) {
        int idx = activity_index_of(k_learningPath[i]);
        if (idx < 0) continue;
        int unlocked = progress_is_unlocked(&app->progress, idx);
        int done = app->progress.completed[idx];
        const Activity *a = &k_activities[idx];
        wchar_t title[64];
        MultiByteToWideChar(CP_UTF8, 0, a->title, -1, title, 64);
        wchar_t line[160];
        swprintf(line, 160, L"%d. %ls %ls", i + 1, title,
                 done ? L" [done]" : (unlocked ? L"" : L" (locked)"));
        RECT rc = {40, y, app->width - 170, y + rowH - 6};
        COLORREF bg = done ? RGB(200, 255, 200) : (unlocked ? RGB(255, 240, 180) : RGB(220, 220, 220));
        fill_rect(hdc, rc, bg);
        draw_text_center(hdc, rc, line, 20, RGB(30, 30, 30), 0);
        RECT play = {app->width - 160, y, app->width - 40, y + rowH - 6};
        if (unlocked) {
            draw_button(app, hdc, play, BTN_ACTIVITY_START, L"Play", idx, RGB(160, 240, 160), 1);
        } else {
            draw_button(app, hdc, play, BTN_NONE, L"Locked", 0, RGB(200, 200, 200), 0);
        }
        y += rowH;
        if (y > app->height - 60) break;
    }
}

/* ---------- screens: activity lists ---------- */
static void render_activity_list(AppState *app, HDC hdc, Domain dom, const wchar_t *title, COLORREF bgc) {
    fill_rect(hdc, (RECT){0, 0, app->width, app->height}, bgc);
    draw_back(app, hdc);
    draw_text_center(hdc, (RECT){140, 10, app->width - 10, 60}, title, 36, RGB(40, 40, 40), 1);
    int y = 80;
    for (int i = 0; i < ACTIVITY_COUNT; i++) {
        if (k_activities[i].domain != dom) continue;
        wchar_t wtitle[64];
        MultiByteToWideChar(CP_UTF8, 0, k_activities[i].title, -1, wtitle, 64);
        wchar_t line[96];
        swprintf(line, 96, L"%ls %ls", wtitle,
                 app->progress.completed[i] ? L"\u2605" : L"");
        RECT rc = {60, y, app->width - 230, y + 64};
        fill_rect(hdc, rc, RGB(255, 255, 255));
        draw_text_center(hdc, rc, line, 22, RGB(30, 30, 30), 0);
        RECT play = {app->width - 220, y, app->width - 60, y + 64};
        draw_button(app, hdc, play, BTN_ACTIVITY_START, L"Play", i, RGB(180, 240, 180), 1);
        y += 74;
        if (y > app->height - 40) break;
    }
}

/* ---------- screens: rewards ---------- */
static void render_rewards(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0, 0, app->width, app->height}, RGB(255, 248, 220));
    draw_back(app, hdc);
    draw_text_center(hdc, (RECT){140, 10, app->width - 10, 60}, L"My Rewards", 36, RGB(80, 60, 20), 1);
    wchar_t buf[128];
    swprintf(buf, 128, L"Stars: %d   Stickers: %d", progress_star_total(&app->progress), progress_sticker_count(&app->progress));
    draw_text_center(hdc, (RECT){0, 72, app->width, 116}, buf, 28, RGB(40, 110, 40), 1);

    int x = 60, y = 140, size = 100, gap = 24, col = 0;
    for (int i = 0; i < STICKER_COUNT; i++) {
        RECT rc = {x, y, x + size, y + size};
        COLORREF c = app->progress.stickers[i] ? RGB(255, 215, 0) : RGB(220, 220, 220);
        fill_rect(hdc, rc, c);
        HPEN pen = CreatePen(PS_SOLID, 3, RGB(80, 80, 80));
        HGDIOBJ op = SelectObject(hdc, pen); HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Ellipse(hdc, rc.left + 8, rc.top + 8, rc.right - 8, rc.bottom - 8);
        SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pen);
        wchar_t lab[8]; swprintf(lab, 8, L"%d", i + 1);
        draw_text_center(hdc, rc, lab, 26, RGB(40, 40, 40), 1);
        if (app->progress.stickers[i]) {
            /* star mark */
            draw_text_center(hdc, (RECT){rc.left, rc.top + size - 30, rc.right, rc.bottom}, L"\u2605", 24, RGB(200, 160, 0), 1);
        }
        x += size + gap; col++;
        if (col >= 5) { col = 0; x = 60; y += size + gap; }
    }
}

/* ---------- screens: settings ---------- */
static void render_settings(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0, 0, app->width, app->height}, RGB(240, 240, 245));
    draw_back(app, hdc);
    draw_text_center(hdc, (RECT){140, 10, app->width - 10, 60}, L"Settings (Parents)", 30, RGB(50, 50, 50), 1);

    RECT mute = {80, 100, 440, 170};
    draw_button(app, hdc, mute, BTN_MUTE, app->progress.muted ? L"\u26A1 Sound: OFF" : L"\u26A1 Sound: ON", 0, RGB(200, 230, 255), 1);
    RECT rm = {80, 190, 440, 260};
    draw_button(app, hdc, rm, BTN_REDUCE_MOTION, app->progress.reduceMotion ? L"\u26F0 Reduce Motion: ON" : L"\u26F0 Reduce Motion: OFF", 0, RGB(200, 230, 255), 1);
    RECT reset = {80, 290, 440, 360};
    draw_button(app, hdc, reset, BTN_RESET, L"\u2716 Reset Progress", 0, RGB(255, 200, 200), 1);

    if (app->showResetConfirm) {
        RECT box = {480, 220, 780, 380};
        fill_rect(hdc, box, RGB(255, 255, 220));
        draw_text_wrap(hdc, (RECT){490, 230, 770, 290}, L"Reset all stars and progress?", 22, RGB(30, 30, 30));
        draw_button(app, hdc, (RECT){500, 300, 610, 350}, BTN_CONFIRM_YES, L"Yes", 0, RGB(255, 180, 180), 1);
        draw_button(app, hdc, (RECT){630, 300, 740, 350}, BTN_CONFIRM_NO, L"No", 0, RGB(200, 255, 200), 1);
    }

    if (app->saveError) {
        draw_text_wrap(hdc, (RECT){80, 350, 440, 390},
            L"Progress save failed. Progress may be lost on close.",
            16, RGB(180, 60, 60));
    }

    draw_text_wrap(hdc, (RECT){80, 390, app->width - 80, app->height - 20},
        L"WonderSprout is 100% free and offline. No accounts, no ads, no tracking, no network. Progress is saved locally on this device only.",
        18, RGB(70, 70, 70));
}

/* ---------- letter drawing helpers ---------- */
/* Draw a big uppercase letter as a series of dots for tracing. */
static const wchar_t *k_letter_words[5] = {L"Apple", L"Ball", L"Cat", L"Dog", L"Elephant"};

static void render_letter_gallery(AppState *app, HDC hdc, int letterIndex) {
    fill_rect(hdc, (RECT){0, 0, app->width, app->height}, BGC_LETTERS);
    render_activity_header(app, hdc);
    wchar_t letter[4] = {L'A' + (wchar_t)letterIndex, 0};
    draw_text_center(hdc, (RECT){0, 100, app->width, 320}, letter, 180, RGB(40, 90, 160), 1);
    wchar_t word[64]; swprintf(word, 64, L"%ls", k_letter_words[letterIndex]);
    draw_text_center(hdc, (RECT){0, 320, app->width, 370}, word, 34, RGB(50, 50, 50), 1);
    /* trace prompt */
    draw_text_center(hdc, (RECT){0, 400, app->width, 430}, L"Trace the letter with your finger!", 20, RGB(80, 80, 80), 0);
    /* dotted path placeholder: draw letter as dots */
    int cx = app->width / 2, cy = 220;
    int baseSz = 100;
    /* draw a dotted outline roughly shaped as the letter */
    HBRUSH br = CreateSolidBrush(RGB(180, 200, 240));
    HGDIOBJ ob = SelectObject(hdc, br);
    for (int i = 0; i < 40; i++) {
        float t = (float)i / 39.0f;
        float dx = 0, dy = 0;
        if (letterIndex == 0) { /* A */
            float a = t * 3.14159f;
            dx = cx + (cosf(a) * 0.5f - 0.5f) * baseSz * 0.7f * (t < 0.5f ? t * 2 : (1 - t) * 2);
            dy = cy - baseSz * 0.8f + t * baseSz * 1.6f;
            if (t < 0.5f) dx = cx - baseSz * 0.4f + t * baseSz * 1.6f;
            else if (t > 0.5f) dx = cx + baseSz * 0.4f - (t - 0.5f) * baseSz * 1.6f;
            dy = cy + baseSz * 0.8f - t * baseSz * 1.6f;
        } else { /* simple curve fallback */
            dx = cx + cosf(t * 6.28f) * baseSz * 0.5f;
            dy = cy + sinf(t * 6.28f) * baseSz * 0.5f;
        }
        RECT dot = {(int)dx - 6, (int)dy - 6, (int)dx + 6, (int)dy + 6};
        fill_rect(hdc, dot, RGB(180, 200, 240));
    }
    SelectObject(hdc, ob); DeleteObject(br);

    RECT fin = {app->width / 2 - 130, 500, app->width / 2 + 130, 560};
    draw_button(app, hdc, fin, BTN_ACTIVITY_ACTION, L"I traced it!", 1, RGB(180, 255, 180), 1);
    if (app->actDone) render_activity_done(app, hdc);
}

static void render_find_letter(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0, 0, app->width, app->height}, BGC_LETTERS);
    render_activity_header(app, hdc);
    wchar_t targets[5] = {L'A', L'B', L'C', L'D', L'E'};
    wchar_t target = targets[app->actStep % 5];
    wchar_t prompt[64]; swprintf(prompt, 64, L"Find the letter: %lc", target);
    draw_text_center(hdc, (RECT){0, 90, app->width, 140}, prompt, 34, RGB(30, 30, 30), 1);
    /* speak target once when step changes */
    static int lastSpeakStep = -1;
    if (app->actStep != lastSpeakStep) {
        wchar_t spk[48]; swprintf(spk, 48, L"Find the letter %lc", target);
        speak(app, spk);
        lastSpeakStep = app->actStep;
    }
    /* 4 unique choices including target; equal styling (no answer hint) */
    wchar_t opts[4]; int count = 0;
    opts[count++] = target;
    for (int d = 1; count < 4 && d < 5; d++) {
        wchar_t cand = L'A' + (wchar_t)(((target - L'A') + d) % 5);
        int found = 0;
        for (int i = 0; i < count; i++) if (opts[i] == cand) { found = 1; break; }
        if (!found) opts[count++] = cand;
    }
    int rot = app->actStep % 4;
    wchar_t shuffled[4];
    for (int i = 0; i < 4; i++) shuffled[i] = opts[(i + rot) % 4];
    int bw = 150, bh = 150, gap = 30, total = 4 * bw + 3 * gap;
    int startX = (app->width - total) / 2, y = 220;
    for (int i = 0; i < 4; i++) {
        RECT rc = {startX + i * (bw + gap), y, startX + i * (bw + gap) + bw, y + bh};
        wchar_t lab[2] = {shuffled[i], 0};
        int isCorrect = (shuffled[i] == target);
        COLORREF fill = RGB(255, 255, 255);
        fill_rect(hdc, rc, fill);
        HPEN pen = CreatePen(PS_SOLID, 3, RGB(120, 120, 120));
        HGDIOBJ op = SelectObject(hdc, pen); HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 20, 20);
        SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pen);
        draw_text_center(hdc, rc, lab, 48, RGB(30, 30, 30), 1);
        hit_add(app, rc, BTN_ACTIVITY_ACTION, lab, isCorrect ? 1 : 0, 1);
    }
    if (app->actDone) render_activity_done(app, hdc);
}

/* ---------- number activities ---------- */
static void render_number_show(AppState *app, HDC hdc, int n) {
    fill_rect(hdc, (RECT){0, 0, app->width, app->height}, BGC_NUMBERS);
    render_activity_header(app, hdc);
    wchar_t num[8]; swprintf(num, 8, L"%d", n);
    draw_text_center(hdc, (RECT){0, 80, app->width / 2, 220}, num, 140, RGB(160, 50, 50), 1);
    /* speak number once per activity entry */
    static int lastNumIdx = -1;
    if (app->activeActIdx != lastNumIdx) {
        wchar_t spk[32]; swprintf(spk, 32, L"Tap %d objects to count to %d", n, n);
        speak(app, spk);
        lastNumIdx = app->activeActIdx;
    }
    /* draw n tap-able circles; actStep is a bitmask of tapped objects */
    int maxPerRow = 5, rows = (n + maxPerRow - 1) / maxPerRow;
    int startY = 260, r = 28, gap = 16;
    int totalW = maxPerRow * (2 * r + gap) - gap;
    int startX = (app->width - totalW) / 2;
    for (int i = 0; i < n; i++) {
        int tapped = (app->actStep & (1 << i)) != 0;
        int row = i / maxPerRow, col = i % maxPerRow;
        int cx = startX + col * (2 * r + gap) + r;
        int cy = startY + row * (2 * r + gap) + r;
        RECT rc = {cx - r, cy - r, cx + r, cy + r};
        COLORREF c = tapped ? RGB(120, 220, 120) : RGB(255, 140, 140);
        fill_rect(hdc, rc, c);
        HPEN pen = CreatePen(PS_SOLID, 3, tapped ? RGB(40, 120, 40) : RGB(120, 40, 40));
        HGDIOBJ op = SelectObject(hdc, pen); HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pen);
        wchar_t d[4]; swprintf(d, 4, L"%d", i + 1);
        draw_text_center(hdc, rc, d, 22, RGB(255, 255, 255), 1);
        hit_add(app, rc, BTN_ACTIVITY_ACTION, L"", i + 1, 1);
    }
    int tappedCount = 0;
    for (int i = 0; i < n; i++) if (app->actStep & (1 << i)) tappedCount++;
    wchar_t prompt[64]; swprintf(prompt, 64, L"Tap each object: %d of %d", tappedCount, n);
    draw_text_center(hdc, (RECT){0, startY + rows * (2 * r + gap) + 20, app->width, startY + rows * (2 * r + gap) + 60},
                     prompt, 20, RGB(80, 80, 80), 0);
    if (app->actDone) render_activity_done(app, hdc);
}

static void render_count_quiz(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0, 0, app->width, app->height}, BGC_NUMBERS);
    render_activity_header(app, hdc);
    int counts[5] = {2, 3, 4, 5, 3};
    int target = counts[app->actStep % 5];
    draw_text_center(hdc, (RECT){0, 80, app->width, 120}, L"How many circles?", 30, RGB(30, 30, 30), 1);
    static int lastQuizTarget = -1;
    if (target != lastQuizTarget) {
        wchar_t spk[32]; swprintf(spk, 32, L"How many circles?");
        speak(app, spk);
        lastQuizTarget = target;
    }
    /* draw target circles */
    int cx0 = app->width / 2 - target * 30;
    for (int i = 0; i < target; i++) {
        int cx = cx0 + i * 60 + 30, cy = 190;
        HBRUSH br = CreateSolidBrush(RGB(100, 160, 255));
        HGDIOBJ ob = SelectObject(hdc, br);
        Ellipse(hdc, cx - 24, cy - 24, cx + 24, cy + 24);
        SelectObject(hdc, ob); DeleteObject(br);
    }
    /* generate 4 unique options including target exactly once */
    int opts[4]; int count = 0;
    opts[count++] = target;
    int delta = 1;
    while (count < 4) {
        int up = target + delta;
        int down = target - delta;
        if (down >= 1 && count < 4) {
            int found = 0;
            for (int i = 0; i < count; i++) if (opts[i] == down) { found = 1; break; }
            if (!found) opts[count++] = down;
        }
        if (up <= 10 && count < 4) {
            int found = 0;
            for (int i = 0; i < count; i++) if (opts[i] == up) { found = 1; break; }
            if (!found) opts[count++] = up;
        }
        delta++;
    }
    /* shuffle positions deterministically but not trivially */
    int rot = app->actStep % 4;
    int shuffled[4];
    for (int i = 0; i < 4; i++) shuffled[i] = opts[(i + rot) % 4];
    int bw = 110, bh = 100, gap = 28, total = 4 * bw + 3 * gap, sx = (app->width - total) / 2, y = 300;
    for (int i = 0; i < 4; i++) {
        wchar_t lab[4]; swprintf(lab, 4, L"%d", shuffled[i]);
        COLORREF fill = RGB(255, 255, 210);
        RECT rc = {sx + i * (bw + gap), y, sx + i * (bw + gap) + bw, y + bh};
        fill_rect(hdc, rc, fill);
        HPEN pen = CreatePen(PS_SOLID, 3, RGB(80, 80, 80));
        HGDIOBJ op = SelectObject(hdc, pen); HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 16, 16);
        SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pen);
        draw_text_center(hdc, rc, lab, 32, RGB(30, 30, 30), 1);
        hit_add(app, rc, BTN_ACTIVITY_ACTION, lab, shuffled[i], 1);
    }
    if (app->actDone) render_activity_done(app, hdc);
}

/* ---------- shape & color activities ---------- */
static const wchar_t *k_shape_names[4] = {L"Circle", L"Square", L"Triangle", L"Star"};

static void render_shape_match(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0, 0, app->width, app->height}, BGC_SHAPES);
    render_activity_header(app, hdc);
    int target = app->actStep % 4;
    wchar_t prompt[64]; swprintf(prompt, 64, L"Tap the %ls", k_shape_names[target]);
    draw_text_center(hdc, (RECT){0, 80, app->width, 120}, prompt, 30, RGB(30, 30, 30), 1);
    static int lastShape = -1;
    if (target != lastShape) {
        speak(app, prompt);
        lastShape = target;
    }
    int bw = 140, bh = 140, gap = 30, total = 4 * bw + 3 * gap, sx = (app->width - total) / 2, y = 200;
    for (int i = 0; i < 4; i++) {
        RECT rc = {sx + i * (bw + gap), y, sx + i * (bw + gap) + bw, y + bh};
        fill_rect(hdc, rc, RGB(255, 255, 240));
        HPEN pen = CreatePen(PS_SOLID, 3, RGB(60, 140, 60));
        HBRUSH br = CreateSolidBrush(RGB(200, 240, 200));
        HGDIOBJ op = SelectObject(hdc, pen); HGDIOBJ ob = SelectObject(hdc, br);
        int cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
        if (i == 0) Ellipse(hdc, cx - 44, cy - 44, cx + 44, cy + 44);
        else if (i == 1) Rectangle(hdc, cx - 44, cy - 44, cx + 44, cy + 44);
        else if (i == 2) { POINT pts[3] = {{cx, cy - 48}, {cx - 48, cy + 38}, {cx + 48, cy + 38}}; Polygon(hdc, pts, 3); }
        else { POINT pts[4] = {{cx, cy - 48}, {cx + 44, cy}, {cx, cy + 48}, {cx - 44, cy}}; Polygon(hdc, pts, 4); }
        SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pen); DeleteObject(br);
        wchar_t lab[24]; swprintf(lab, 24, L"%ls", k_shape_names[i]);
        hit_add(app, rc, BTN_ACTIVITY_ACTION, lab, (i == target) ? 1 : 0, 1);
    }
    if (app->actDone) render_activity_done(app, hdc);
}

/* ---------- music activities ---------- */
static float k_note_freqs[8] = {261.63f, 293.66f, 329.63f, 349.23f, 392.00f, 440.00f, 493.88f, 523.25f};
static const wchar_t *k_note_names[8] = {L"C", L"D", L"E", L"F", L"G", L"A", L"B", L"C2"};
static COLORREF k_note_colors[8] = {RGB(255,120,120), RGB(255,190,120), RGB(255,255,120), RGB(160,255,160), RGB(140,210,255), RGB(180,160,255), RGB(230,140,255), RGB(255,120,220)};

static void render_music_pads(AppState *app, HDC hdc) {
    fill_rect(hdc, (RECT){0, 0, app->width, app->height}, BGC_MUSIC);
    render_activity_header(app, hdc);
    draw_text_center(hdc, (RECT){0, 80, app->width, 116}, L"Tap the colorful pads to play notes!", 22, RGB(60, 60, 60), 0);
    int bw = 96, bh = 150, gap = 14, total = 8 * bw + 7 * gap, sx = (app->width - total) / 2, y = 160;
    for (int i = 0; i < 8; i++) {
        RECT rc = {sx + i * (bw + gap), y, sx + i * (bw + gap) + bw, y + bh};
        COLORREF fill = k_note_colors[i];
        fill_rect(hdc, rc, fill);
        HPEN pen = CreatePen(PS_SOLID, 4, RGB(60, 60, 60));
        HGDIOBJ op = SelectObject(hdc, pen); HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 16, 16);
        SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pen);
        draw_text_center(hdc, rc, k_note_names[i], 24, RGB(255, 255, 255), 1);
        hit_add(app, rc, (ButtonId)(BTN_PAD_1 + i), k_note_names[i], i, 1);
    }
    RECT done = {app->width / 2 - 110, 360, app->width / 2 + 110, 420};
    draw_button(app, hdc, done, BTN_ACTIVITY_ACTION, L"Done playing", 1, RGB(200, 255, 200), 1);
    if (app->actDone) render_activity_done(app, hdc);
}

/* ---------- activity renderer ---------- */
void app_render(AppState *app, HDC hdc) {
    hits_clear(app);
    switch (app->screen) {
        case SCREEN_HOME: render_home(app, hdc); break;
        case SCREEN_PATH: render_path(app, hdc); break;
        case SCREEN_LETTERS: render_activity_list(app, hdc, DOMAIN_LETTERS, L"Letters", BGC_LETTERS); break;
        case SCREEN_NUMBERS: render_activity_list(app, hdc, DOMAIN_NUMBERS, L"Numbers", BGC_NUMBERS); break;
        case SCREEN_SHAPES:  render_activity_list(app, hdc, DOMAIN_SHAPES_COLORS, L"Shapes & Colors", BGC_SHAPES); break;
        case SCREEN_MUSIC:   render_activity_list(app, hdc, DOMAIN_MUSIC, L"Music", BGC_MUSIC); break;
        case SCREEN_REWARDS: render_rewards(app, hdc); break;
        case SCREEN_SETTINGS:render_settings(app, hdc); break;
        case SCREEN_ACTIVITY:
            if (app->activeActIdx < 0) { render_home(app, hdc); break; }
            {
                const char *id = k_activities[app->activeActIdx].id;
                if (strcmp(id, "LT_A") == 0 || strcmp(id, "LT_B") == 0 || strcmp(id, "LT_C") == 0 ||
                    strcmp(id, "LT_D") == 0 || strcmp(id, "LT_E") == 0) {
                    int li = (int)(id[3] - 'A');
                    render_letter_gallery(app, hdc, li >= 0 && li < 5 ? li : 0);
                } else if (strcmp(id, "LT_FIND") == 0 || strcmp(id, "LT_TRACE") == 0) {
                    render_find_letter(app, hdc);
                } else if (strcmp(id, "NM_1") == 0) render_number_show(app, hdc, 1);
                else if (strcmp(id, "NM_2") == 0) render_number_show(app, hdc, 2);
                else if (strcmp(id, "NM_3") == 0) render_number_show(app, hdc, 3);
                else if (strcmp(id, "NM_4") == 0) render_number_show(app, hdc, 4);
                else if (strcmp(id, "NM_5") == 0) render_number_show(app, hdc, 5);
                else if (strcmp(id, "NM_COUNT") == 0 || strcmp(id, "NM_MATCH") == 0) render_count_quiz(app, hdc);
                else if (strcmp(id, "SC_SHAPE") == 0 || strcmp(id, "SC_COLOR") == 0 || strcmp(id, "SC_SORT") == 0) render_shape_match(app, hdc);
                else if (strcmp(id, "MU_PADS") == 0 || strcmp(id, "MU_SONG1") == 0 || strcmp(id, "MU_SONG2") == 0) render_music_pads(app, hdc);
                else { fill_rect(hdc, (RECT){0, 0, app->width, app->height}, RGB(240, 240, 240)); render_activity_header(app, hdc); draw_button(app, hdc, (RECT){app->width / 2 - 100, 300, app->width / 2 + 100, 360}, BTN_ACTIVITY_ACTION, L"Finish", 1, RGB(200, 255, 200), 1); if (app->actDone) render_activity_done(app, hdc); }
            }
            break;
        default: render_home(app, hdc); break;
    }
    particle_update();
    particle_draw(hdc);
}

static void render_activity_header(AppState *app, HDC hdc) {
    draw_back(app, hdc);
    if (app->activeActIdx < 0) return;
    const Activity *a = &k_activities[app->activeActIdx];
    wchar_t title[128], cue[256];
    MultiByteToWideChar(CP_UTF8, 0, a->title, -1, title, 128);
    MultiByteToWideChar(CP_UTF8, 0, a->cue ? a->cue : "", -1, cue, 256);
    draw_text_center(hdc, (RECT){140, 4, app->width - 10, 44}, title, 28, RGB(30, 30, 30), 1);
    draw_text_center(hdc, (RECT){140, 40, app->width - 10, 72}, cue, 20, RGB(80, 80, 80), 0);
    /* speak cue when entering activity */
    static int lastAct = -1;
    if (app->activeActIdx != lastAct) {
        speak(app, cue);
        lastAct = app->activeActIdx;
    }
}

static void render_activity_done(AppState *app, HDC hdc) {
    RECT box = {app->width / 2 - 220, app->height / 2 - 110, app->width / 2 + 220, app->height / 2 + 130};
    fill_rect(hdc, box, RGB(255, 245, 180));
    HPEN pen = CreatePen(PS_SOLID, 4, RGB(200, 160, 0));
    HGDIOBJ op = SelectObject(hdc, pen); HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, box.left, box.top, box.right, box.bottom, 24, 24);
    SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pen);
    draw_text_center(hdc, (RECT){box.left, box.top + 14, box.right, box.top + 64}, L"Great job! Star earned!", 26, RGB(40, 130, 40), 1);
    draw_button(app, hdc, (RECT){box.left + 30, box.top + 86, box.right - 30, box.top + 146}, BTN_BACK, L"Back", 0, RGB(180, 240, 180), 1);
}

/* ---------- click handling ---------- */
static void handle_activity_action(AppState *app, int param) {
    if (app->activeActIdx < 0) return;
    const char *id = k_activities[app->activeActIdx].id;

    /* Letter galleries: tapping "I know it!" completes */
    if (strcmp(id, "LT_A") == 0 || strcmp(id, "LT_B") == 0 || strcmp(id, "LT_C") == 0 ||
        strcmp(id, "LT_D") == 0 || strcmp(id, "LT_E") == 0) {
        app->actScore = 3;
        finish_activity(app);
        return;
    }

    /* Number intros: require all N objects tapped (param = 1..n, bitmask in actStep) */
    if (strcmp(id, "NM_1") == 0 || strcmp(id, "NM_2") == 0 || strcmp(id, "NM_3") == 0 ||
        strcmp(id, "NM_4") == 0 || strcmp(id, "NM_5") == 0) {
        int n = id[3] - '0';
        if (param >= 1 && param <= n) {
            int bit = 1 << (param - 1);
            if (!(app->actStep & bit)) {
                app->actStep |= bit;
                wchar_t spk[16]; swprintf(spk, 16, L"%d", param);
                speak(app, spk);
                int tapped = 0;
                for (int i = 0; i < n; i++) if (app->actStep & (1 << i)) tapped++;
                if (tapped >= n) { app->actScore = 3; finish_activity(app); }
                else set_status(app, L"Keep tapping!");
            }
        }
        return;
    }

    if (strcmp(id, "LT_FIND") == 0 || strcmp(id, "LT_TRACE") == 0) {
        if (param == 1) {
            app->actStep++;
            play_tone(app, 700.0f, 0.1f);
            if (app->actStep >= 3) { app->actScore = 3; finish_activity(app); }
            else set_status(app, L"Nice! Find the next one.");
        } else {
            play_feedback(app, 0);
            set_status(app, L"Try again! Look for the letter.");
        }
        return;
    }

    if (strcmp(id, "NM_COUNT") == 0 || strcmp(id, "NM_MATCH") == 0) {
        int counts[5] = {2, 3, 4, 5, 3};
        int target = counts[app->actStep % 5];
        if (param == target) {
            app->actStep++;
            play_tone(app, 720.0f, 0.1f);
            if (app->actStep >= 3) { app->actScore = 3; finish_activity(app); }
            else set_status(app, L"Correct! Great counting!");
        } else {
            play_feedback(app, 0);
            set_status(app, L"Count again: how many circles?");
        }
        return;
    }

    if (strcmp(id, "SC_SHAPE") == 0 || strcmp(id, "SC_COLOR") == 0 || strcmp(id, "SC_SORT") == 0) {
        if (param == 1) {
            app->actStep++;
            play_tone(app, 740.0f, 0.1f);
            if (app->actStep >= 3) { app->actScore = 3; finish_activity(app); }
            else set_status(app, L"Great match!");
        } else {
            play_feedback(app, 0);
            set_status(app, L"Try another one!");
        }
        return;
    }

    if (strcmp(id, "MU_PADS") == 0 || strcmp(id, "MU_SONG1") == 0 || strcmp(id, "MU_SONG2") == 0) {
        app->actScore = 2;
        finish_activity(app);
        return;
    }

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
        case BTN_HOME_SETTINGS: app->screen = SCREEN_SETTINGS; app->showResetConfirm = 0; return 1;

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
            if (storage_reset(app->savePath) == 0) {
                storage_load(app->savePath, &app->progress);
                app->showResetConfirm = 0;
                app->saveError = 0;
                set_status(app, L"Progress reset.");
                speak(app, L"Progress reset.");
            } else {
                app->showResetConfirm = 0;
                app->saveError = 1;
                set_status(app, L"Reset failed — progress may not be saved.");
                speak(app, L"Reset failed.");
            }
            return 1;

        case BTN_CONFIRM_NO:
            app->showResetConfirm = 0;
            return 1;

        case BTN_ACTIVITY_START:
            if (param >= 0 && param < ACTIVITY_COUNT) start_activity(app, param);
            return 1;

        case BTN_PAD_1: case BTN_PAD_2: case BTN_PAD_3: case BTN_PAD_4:
        case BTN_PAD_5: case BTN_PAD_6: case BTN_PAD_7: case BTN_PAD_8: {
            int idx = (int)id - (int)BTN_PAD_1;
            if (idx >= 0 && idx < 8) {
                play_tone(app, k_note_freqs[idx], 0.3f);
                /* brief visual highlight handled via actStep */
                app->actStep = (app->actStep + 1) % 10;
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
