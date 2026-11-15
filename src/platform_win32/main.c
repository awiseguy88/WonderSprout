#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <string.h>

#include "app_state.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

static AppState g_app;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        app_init(&g_app);
        {
            RECT rc;
            GetClientRect(hwnd, &rc);
            app_layout(&g_app, rc.right - rc.left, rc.bottom - rc.top);
        }
        SetTimer(hwnd, 1, 100, NULL);
        return 0;

    case WM_SIZE:
        app_layout(&g_app, LOWORD(lParam), HIWORD(lParam));
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        if (app_click(&g_app, x, y)) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_TIMER:
        /* Light periodic repaint for animations/status; kept cheap. */
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_ERASEBKGND:
        return 1; /* we paint everything in WM_PAINT */

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        /* Double buffer to avoid flicker. */
        HDC memdc = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, g_app.width > 0 ? g_app.width : 1, g_app.height > 0 ? g_app.height : 1);
        HBITMAP old = (HBITMAP)SelectObject(memdc, bmp);
        app_render(&g_app, memdc);
        BitBlt(hdc, 0, 0, g_app.width, g_app.height, memdc, 0, 0, SRCCOPY);
        SelectObject(memdc, old);
        DeleteObject(bmp);
        DeleteDC(memdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        app_shutdown(&g_app);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hPrev; (void)lpCmd;

    /* DPI awareness for crisp rendering. */
    SetProcessDPIAware();

    const wchar_t CLASS_NAME[] = L"WonderSproutWindow";

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc)) return 1;

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"WonderSprout - Free Learning for Kids",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 720,
        NULL, NULL, hInst, NULL);

    if (!hwnd) return 1;

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
