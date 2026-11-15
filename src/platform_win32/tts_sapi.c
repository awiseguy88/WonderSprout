#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <sapi.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef _MSC_VER
#pragma comment(lib, "sapi.lib")
#pragma comment(lib, "ole32.lib")
#endif

static ISpVoice *g_pVoice = NULL;
static bool g_initialized = false;

int tts_init(void) {
    if (g_initialized) return 0;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) return -1;
    hr = CoCreateInstance(&CLSID_SpVoice, NULL, CLSCTX_ALL, &IID_ISpVoice, (void **)&g_pVoice);
    if (FAILED(hr) || !g_pVoice) {
        CoUninitialize();
        return -1;
    }
    /* Set a child-friendly rate (slightly slower) */
    g_pVoice->lpVtbl->SetRate(g_pVoice, -1);
    g_initialized = true;
    return 0;
}

void tts_shutdown(void) {
    if (!g_initialized) return;
    if (g_pVoice) {
        g_pVoice->lpVtbl->Speak(g_pVoice, L"", SPF_PURGEBEFORESPEAK, NULL);
        g_pVoice->lpVtbl->Release(g_pVoice);
        g_pVoice = NULL;
    }
    CoUninitialize();
    g_initialized = false;
}

int tts_speak(const wchar_t *text, bool async) {
    if (!g_initialized || !g_pVoice || !text) return -1;
    DWORD flags = SPF_IS_XML;
    if (async) flags |= SPF_ASYNC;
    /* if not async, speak synchronously (no SPF_ASYNC flag) */
    HRESULT hr = g_pVoice->lpVtbl->Speak(g_pVoice, text, flags, NULL);
    return SUCCEEDED(hr) ? 0 : -1;
}

void tts_stop(void) {
    if (!g_initialized || !g_pVoice) return;
    g_pVoice->lpVtbl->Speak(g_pVoice, L"", SPF_PURGEBEFORESPEAK, NULL);
}

void tts_set_rate(int rate) {
    if (!g_initialized || !g_pVoice) return;
    if (rate < -10) rate = -10;
    if (rate > 10) rate = 10;
    g_pVoice->lpVtbl->SetRate(g_pVoice, rate);
}

bool tts_available(void) {
    return g_initialized && g_pVoice != NULL;
}