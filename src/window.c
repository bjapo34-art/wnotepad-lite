#include <windows.h>
#include "resources.h"

extern void OnSize(HWND hwnd);
extern void ManejarComando(HWND hwnd, WPARAM wParam, HINSTANCE hInst);
extern BOOL AlCerrarConfirmar(HWND hwnd);

LRESULT CALLBACK WndProcPrincipal(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        return 0;
    case WM_SIZE:
        OnSize(hwnd);
        return 0;
    case WM_COMMAND:
        ManejarComando(hwnd, wp, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE));
        return 0;
    case WM_CLOSE:
        if (AlCerrarConfirmar(hwnd)) DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void OnSize(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    MoveWindow(GetDlgItem(hwnd, IDC_EDIT), 0, 0, rc.right, rc.bottom, TRUE);
}

// Placeholder: sin detección de cambios todavía
BOOL AlCerrarConfirmar(HWND hwnd)
{
    (void)hwnd;
    return TRUE;
}