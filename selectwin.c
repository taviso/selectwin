#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "gdi32")
#pragma comment(lib, "user32")

// Select a window interactively from a script.
//
// Tavis Ormandy <taviso@gmail.com>, December 2025
//

// The size of the frame drawn around selected windows.
static const DWORD kBorderWidth = 6;

// The color of the frame, note that it cannot be black as we key that out with LWA_COLORKEY.
static const COLORREF kBorderColor = RGB(255, 0, 0);

// How often (ms) we check for a new window target.
static const DWORD kCheckInterval = 50;

// GDI objects to draw the overlay.
static HBRUSH bg;
static HPEN border;

// The overlay handle.
static HWND overlay;

// The window we think the user is pointing at.
static HWND target;

// This is used to detect when the user has selected a window.
static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (wParam == WM_LBUTTONDOWN) {
        PostQuitMessage(0);
        // This swallows the click, so that the window isn't raised or activated.
        return 1;
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// Figure out which window the user is pointing at.
static void UpdateTarget() {
    RECT pos, rc;
    POINT pt;

    if (GetCursorPos(&pt) == FALSE)
        goto notarget;

    // We probably only care about toplevel windows, not child windows.
    target = GetAncestor(WindowFromPoint(pt), GA_ROOT);

    // Find dimensions of the window and overlay.
    if (!GetWindowRect(target, &rc) || !GetWindowRect(overlay, &pos))
        goto notarget;

    // Check if we need to move the overlay.
    if (EqualRect(&pos, &rc) || !CopyRect(&pos, &rc))
        return;

    // We need to move it, so configure the overlay.
    SetWindowPos(overlay,
                 HWND_TOPMOST,
                 rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // Repaint the window.
    InvalidateRect(overlay, NULL, TRUE);
    UpdateWindow(overlay);
    return;

  notarget:
    // There is no current target, so hide the window.
    ShowWindow(overlay, SW_HIDE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    CHAR Caption[1024] = {0};
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;

    if (msg == WM_TIMER) {
        UpdateTarget();
        return 0;
    }

    if (msg == WM_PAINT) {
        hdc = BeginPaint(hwnd, &ps);

        GetClientRect(hwnd, &rc);

        // Draw the border. Note that the bg will be keyed out, and
        // the border uses PS_INSIDEFRAME. We can select both here
        // because a dc can can have a brush and a pen selected.
        SelectObject(hdc, bg);
        SelectObject(hdc, border);
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

        // Optionally, draw a caption.
        if (GetWindowTextA(target, Caption, sizeof(Caption) - 1)) {
            SetTextColor(hdc, kBorderColor);
            InflateRect(&rc, -kBorderWidth, -kBorderWidth);
            DrawText(hdc, Caption, -1, &rc, DT_TOP | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

int main(int argc, char **argv) {
    HHOOK MouseHook;
    char handle[32];
    MSG msg;
    WNDCLASS wc = {
        .lpfnWndProc    = WndProc,
        .hInstance      = GetModuleHandle(NULL),
        .lpszClassName  = "HighlightOverlay",
    };

    if (argc == 1) {
        fprintf(stderr, "usage: %s program.exe @@\n", *argv);
        fprintf(stderr, "The @@ will be replaced with the handle\n", *argv);
        fprintf(stderr, "Use `@@` to just print to stdout.\n", *argv);
        return 1;
    }

    bg      = CreateSolidBrush(0);
    border  = CreatePen(PS_INSIDEFRAME, kBorderWidth, kBorderColor);

    SetProcessDPIAware();
    RegisterClass(&wc);

    overlay = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        "HighlightOverlay",
        NULL,
        WS_POPUP,
        0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL
    );

    // Hide the contents of the overlay.
    SetLayeredWindowAttributes(overlay, 0, 0, LWA_COLORKEY);

    // Start a time that we use to check where the pointer is.
    SetTimer(overlay, 1, kCheckInterval, NULL);

    // Prevent immediate selection if mouse is already down
    while (GetAsyncKeyState(VK_LBUTTON) & 0x8000) Sleep(10);

    MouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);

    // Begin processing messages.
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // User has selected a window, cleanup.
    KillTimer(overlay, 1);
    UnhookWindowsHookEx(MouseHook);
    ShowWindow(overlay, SW_HIDE);

    DeleteObject(border);
    DeleteObject(bg);

    DestroyWindow(overlay);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    if (target == NULL) {
        fprintf(stderr, "No window selected\n");
        return 1;
    }

    sprintf(handle, "%#0lx", (ULONG) target);

    if (strcmp(argv[1], "@@") == 0) {
        puts(handle);
        return 0;
    }

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "@@") == 0)
            argv[i] = handle;
    }

    execvp(argv[1], &argv[1]);
    fprintf(stderr, "error: execvp of %s failed\n", argv[1]);
    return 1;
}
