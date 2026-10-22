#include <windows.h>
#include "app.h"
#include "resources.h"

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
        LineasActualizar(DocsEditActivo());
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
    /* Primero se coloca la barra de pestañas y la barra de estado (que
       definen el alto del área de edición) y después se reacomodan el
       panel y el editor. */
    PestanasReflow(hwnd);
    ReflowBarraEstado(hwnd);
    LineasReflow(hwnd);
}

/* ---------------------------------------------------------------
   AlCerrarConfirmar: pregunta por los cambios sin guardar de cada
   documento abierto antes de cerrar la aplicación
   --------------------------------------------------------------- */
BOOL AlCerrarConfirmar(HWND hwnd)
{
    return DocsCerrarTodos(hwnd, FALSE);
}
