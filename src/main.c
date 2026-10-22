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

static LRESULT CALLBACK EditSubclass(HWND, UINT, WPARAM, LPARAM,
                                      UINT_PTR, DWORD_PTR);

/* Mensajes del EDIT que pueden cambiar el desplazamiento vertical,
   la línea del cursor o el número de líneas del documento. */
static BOOL EsMensajeDeSincronizacion(UINT msg)
{
    switch (msg) {
    case WM_KEYDOWN: case WM_KEYUP: case WM_CHAR:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_MOUSEWHEEL: case WM_SETFOCUS: case WM_SETTEXT:
    case WM_CUT: case WM_PASTE: case WM_CLEAR:
    case EM_SETSEL: case EM_SCROLLCARET: case EM_REPLACESEL:
    case EM_UNDO: case EM_EMPTYUNDOBUFFER:
        return TRUE;
    }
    return FALSE;
}

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

/* Mensajes del EDIT que pueden cambiar la posición del cursor
   o el contenido del documento (necesarios para refrescar la barra
   de estado). */
static BOOL EsMensajeDeCaret(UINT msg)
{
    switch (msg) {
    case WM_KEYDOWN: case WM_KEYUP: case WM_CHAR:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_MOUSEMOVE:
    case WM_SETTEXT: case WM_CUT: case WM_PASTE: case WM_CLEAR:
    case EM_SETSEL: case EM_SCROLLCARET: case EM_REPLACESEL:
        return TRUE;
    }
    return FALSE;
}

/* Mensajes del EDIT que pueden cambiar el texto del documento. Cuando
   llega uno hay que invalidar la caché de líneas lógicas (lineas.c)
   antes de que el EDIT lo procese. */
static BOOL EsMensajeDeContenido(UINT msg, WPARAM wp)
{
    switch (msg) {
    case WM_CHAR: case WM_IME_CHAR:
    case WM_SETTEXT: case WM_CUT: case WM_PASTE: case WM_CLEAR:
    case WM_UNDO: case EM_REPLACESEL: case EM_UNDO:
        return TRUE;
    case WM_KEYDOWN:
        /* Borrar, Insert (puede pegar con Mayúsculas) y los atajos de
           pegar/cortar/deshacer/rehacer con Ctrl */
        if (wp == VK_DELETE || wp == VK_BACK || wp == VK_INSERT)
            return TRUE;
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            switch (wp) {
            case 'V': case 'X': case 'Z': case 'Y':
            case VK_INSERT: case VK_DELETE: case VK_BACK:
                return TRUE;
            }
        }
        return FALSE;
    }
    return FALSE;
}

/* ---------------------------------------------------------------
   EditSubclass: mantiene sincronizados los números de línea y la
   barra de estado con el editor, y atiende los atajos de teclado.
   Cada documento tiene su propio EDIT, así que cada uno lleva su
   subclase (SetWindowSubclass guarda el procedimiento original por
   control, a diferencia de SetWindowLongPtr).
   --------------------------------------------------------------- */
static LRESULT CALLBACK EditSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                      UINT_PTR idSubclass, DWORD_PTR dwRefData)
{
    UNREFERENCED_PARAMETER(idSubclass);
    UNREFERENCED_PARAMETER(dwRefData);

    switch (msg) {
    case WM_KEYDOWN:
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            HWND hParent = GetParent(hwnd);

            /* Ctrl+Tab / Ctrl+Mayús+Tab: cambiar de pestaña */
            if (wp == VK_TAB) {
                BOOL atras = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                DocsCambiarActivo(atras ? -1 : 1);
                SetFocus(DocsEditActivo());
                return 0;
            }

            /* Ctrl+1..9: ir a la pestaña N */
            if (wp >= '1' && wp <= '9') {
                DocsSeleccionar((int)(wp - '1'));
                SetFocus(DocsEditActivo());
                return 0;
            }

            switch (wp) {
            case 'N': PostMessage(hParent, WM_COMMAND, IDM_ARCHIVO_NUEVO, 0); return 0;
            case 'O': PostMessage(hParent, WM_COMMAND, IDM_ARCHIVO_ABRIR, 0); return 0;
            case 'S':
                if (GetKeyState(VK_SHIFT) & 0x8000)
                    PostMessage(hParent, WM_COMMAND, IDM_ARCHIVO_GUARDAR_COMO, 0);
                else
                    PostMessage(hParent, WM_COMMAND, IDM_ARCHIVO_GUARDAR, 0);
                return 0;
            case 'G': PostMessage(hParent, WM_COMMAND, IDM_EDIT_IR_LINEA, 0); return 0;
            case 'W': PostMessage(hParent, WM_COMMAND, IDM_ARCHIVO_CERRAR_PESTANA, 0); return 0;
            }
        }
        break;

    case WM_CHAR:
        /* Ctrl+Tab no debe insertar nada en el texto */
        if (wp == VK_TAB && (GetKeyState(VK_CONTROL) & 0x8000))
            return 0;
        if (wp == VK_ESCAPE) {
            PostMessage(GetParent(hwnd), WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    }

    /* El texto está a punto de cambiar: la caché de líneas lógicas deja
       de ser válida (se recarga en la siguiente consulta) */
    if (EsMensajeDeContenido(msg, wp))
        LineasInvalidarTexto(hwnd);

    LRESULT res = DefSubclassProc(hwnd, msg, wp, lp);

    /* El EDIT ya ha aplicado el cambio, pero durante su procesamiento
       pueden llegar notificaciones (EN_CHANGE, etc.) que consulten el
       texto a medias y dejen la caché con un estado intermedio. Se
       invalida otra vez para que la siguiente consulta lea el texto
       definitivo (por ejemplo, al pegar un bloque de varias líneas). */
    if (EsMensajeDeContenido(msg, wp))
        LineasInvalidarTexto(hwnd);

    /* Mantener el panel de números de línea sincronizado con el editor
       (sólo importa el documento activo) */
    if (hwnd == DocsEditActivo() && EsMensajeDeSincronizacion(msg))
        LineasActualizar(hwnd);

    /* El EDIT es quien sabe si el documento tiene cambios sin guardar:
       se anota en el modelo y se repinta la barra de pestañas para que
       aparezca (o desaparezca) el punto que lo indica */
    if (hwnd == DocsEditActivo() && EsMensajeDeSincronizacion(msg)) {
        int indice = DocsActivo();
        if (indice >= 0) {
            BOOL modificado = (BOOL)SendMessage(hwnd, EM_GETMODIFY, 0, 0);
            if (modificado != DocsModificado(indice)) {
                DocsPonerModificado(indice, modificado);
                PestanasActualizar(GetParent(hwnd));
            }
        }
    }

    /* Actualizar la barra de estado con la posición del cursor */
    if (EsMensajeDeCaret(msg)) {
        HWND hParent = GetParent(hwnd);
        if (hParent) ActualizarBarraEstado(hParent);
    }

    return res;
}

void InstalarSubclaseEdit(HWND hEdit)
{
    SetWindowSubclass(hEdit, EditSubclass, 1, 0);
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
        0, wc.lpszClassName, L"Sin título - wnotepad lite",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 650,
        NULL, ConstruirMenuPrincipal(), hInst, NULL);

    if (!hwnd) return NULL;

    /* Barra de pestañas: ocupa la franja superior del área de cliente */
    CrearPestanas(hwnd, hInst);

    /* Primer documento (vacío): cada documento tiene su propio EDIT */
    DocsCrear(hwnd, L"", CODIF_UTF8);

    /* El editor nace con una tipografía monoespaciada en lugar de la
       fuente proporcional del sistema. Se aplica antes de crear el
       panel de números para que herede la misma tipografía. */
    AplicarFuentePorDefecto(hwnd);

    /* Panel de números de línea a la izquierda del editor */
    CrearPanelNumeros(hwnd, hInst);
    CrearBarraEstado(hwnd, hInst);

    /* Activar el primer documento: muestra su EDIT y refresca la
       barra de pestañas, el título y la barra de estado */
    DocsSeleccionar(0);

    PestanasReflow(hwnd);
    ReflowBarraEstado(hwnd);
    LineasReflow(hwnd);
    ActualizarBarraEstado(hwnd);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return hwnd;
}
