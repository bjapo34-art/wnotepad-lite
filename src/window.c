#include <windows.h>
#include "resources.h"

extern void OnSize(HWND hwnd);
extern void ManejarComando(HWND hwnd, WPARAM wParam, HINSTANCE hInst);
extern BOOL AlCerrarConfirmar(HWND hwnd);
extern void LineasActualizar(HWND hEdit);
extern void LineasReflow(HWND hwndPrincipal);
extern void ActualizarBarraEstado(HWND hwnd);
extern void ReflowBarraEstado(HWND hwnd);

LRESULT CALLBACK WndProcPrincipal(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        return 0;
    case WM_SIZE:
        OnSize(hwnd);
        ActualizarBarraEstado(hwnd);
        return 0;
    case WM_COMMAND:
        ManejarComando(hwnd, wp, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE));
        ActualizarBarraEstado(hwnd);
        return 0;
    case WM_VSCROLL:
        /* La barra de desplazamiento del EDIT notifica a la ventana padre:
           hay que re-sincronizar los números de línea. */
        LineasActualizar(GetDlgItem(hwnd, IDC_EDIT));
        ActualizarBarraEstado(hwnd);
        return 0;
    case WM_NOTIFY:
        ActualizarBarraEstado(hwnd);
        return 0;
    case WM_MOUSEMOVE:
        ActualizarBarraEstado(hwnd);
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
    /* Primero se coloca la barra de estado (define el alto del área de
       edición) y después se reacomodan el panel y el editor. */
    ReflowBarraEstado(hwnd);
    LineasReflow(hwnd);
}

// Placeholder: sin detección de cambios todavía
BOOL AlCerrarConfirmar(HWND hwnd)
{
    (void)hwnd;
    return TRUE;
}