#include <windows.h>
#include "resources.h"

// Declaraciones cruzadas entre módulos
HWND  CrearVentanaPrincipal(HINSTANCE hInst, int nCmdShow);
HMENU ConstruirMenuPrincipal(void);
void  ManejarComando(HWND hwnd, WPARAM wParam, HINSTANCE hInst);
void  OnSize(HWND hwnd);
LRESULT CALLBACK EditProc(HWND, UINT, WPARAM, LPARAM, HINSTANCE);

static WNDPROC g_EditProcOriginal = NULL;
HINSTANCE g_hInst;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    g_hInst = hInstance;

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    HWND hwnd = CrearVentanaPrincipal(hInstance, nCmdShow);
    if (!hwnd) return 1;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        // Aceleraradores del diálogo si hay alguno modal... (por ahora simple)
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// Subclase del EDIT: intercepta teclas para comandos rápidos
static LRESULT CALLBACK EditSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CHAR:
        if (wp == VK_ESCAPE) { PostMessage(GetParent(hwnd), WM_CLOSE, 0, 0); return 0; }
        break;
    case WM_KEYDOWN:
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            switch (wp) {
            case 'N': PostMessage(GetParent(hwnd), WM_COMMAND, IDM_ARCHIVO_NUEVO, 0); return 0;
            case 'O': PostMessage(GetParent(hwnd), WM_COMMAND, IDM_ARCHIVO_ABRIR, 0); return 0;
            case 'S': PostMessage(GetParent(hwnd), WM_COMMAND, IDM_ARCHIVO_GUARDAR, 0); return 0;
            }
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
    wc.lpfnWndProc   = DefWindowProc; // usaremos wndproc propio abajo
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"NotepadWin32Class";
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        0, wc.lpszClassName, L"Sin título - Bloc de notas",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 650,
        NULL, ConstruirMenuPrincipal(), hInst, NULL);
    if (!hwnd) return NULL;

    // EDIT hijo
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

// WndProc principal en window.c (se muestra ahí)