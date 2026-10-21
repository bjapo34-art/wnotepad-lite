/* main.c - Punto de entrada y creación de la ventana principal */

#ifndef _WIN32_IE
#define _WIN32_IE    0x0600
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include <commctrl.h>
#include "app.h"
#include "resources.h"

/* Respaldo por si el SDK no expone esta constante */
#ifndef EM_SETUNDOLIMIT
#define EM_SETUNDOLIMIT 0x00D6
#endif

HINSTANCE g_hInst;
static WNDPROC g_EditProcOriginal = NULL;

static LRESULT CALLBACK EditSubclass(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    g_hInst = hInstance;

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    HWND hwnd = CrearVentanaPrincipal(hInstance, nCmdShow);
    if (!hwnd) return 1;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

static LRESULT CALLBACK EditSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_KEYDOWN:
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            HWND hParent = GetParent(hwnd);
            switch (wp) {
            case 'N': PostMessage(hParent, WM_COMMAND, IDM_ARCHIVO_NUEVO, 0); return 0;
            case 'O': PostMessage(hParent, WM_COMMAND, IDM_ARCHIVO_ABRIR, 0); return 0;
            case 'S': PostMessage(hParent, WM_COMMAND, IDM_ARCHIVO_GUARDAR, 0); return 0;
            case 'G': PostMessage(hParent, WM_COMMAND, IDM_EDIT_IR_LINEA, 0); return 0;
            }
        }
        break;
    case WM_CHAR:
        if (wp == VK_ESCAPE) {
            PostMessage(GetParent(hwnd), WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    }
    return CallWindowProc(g_EditProcOriginal, hwnd, msg, wp, lp);
}

void InstalarSubclaseEdit(HWND hEdit)
{
    g_EditProcOriginal = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclass);
}

HWND CrearVentanaPrincipal(HINSTANCE hInst, int nCmdShow)
{
    WNDCLASSEX wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProcPrincipal;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"NotepadWin32Class";

    if (!RegisterClassEx(&wc)) return NULL;

    HWND hwnd = CreateWindowEx(
        0, wc.lpszClassName, L"Sin título - Bloc de notas",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 650,
        NULL, ConstruirMenuPrincipal(), hInst, NULL);

    if (!hwnd) return NULL;

    HWND hEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL,
        0, 0, 0, 0, hwnd, (HMENU)IDC_EDIT, hInst, NULL);

    SendMessage(hEdit, EM_SETUNDOLIMIT, 100, 0);
    InstalarSubclaseEdit(hEdit);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return hwnd;
}