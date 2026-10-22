/* linenumbers.c - Panel lateral con los números de línea (estilo Notepad++ / Sublime Text)
 *
 * El panel es un control hijo del EDIT principal. Dibuja el número de cada
 * línea del documento, sincronizado con:
 *   - el desplazamiento vertical del EDIT (EM_GETFIRSTVISIBLELINE)
 *   - la línea donde está el cursor (EM_LINEFROMCHAR)
 *
 * El panel no tiene barra de desplazamiento propia: siempre muestra el
 * rango de líneas que el EDIT tiene a la vista.
 */

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include "app.h"
#include "resources.h"

#define CLASE_NUMEROS L"NotepadNumerosLinea"

/* Paleta del panel */
#define COLOR_FONDO_NUMEROS  RGB(240, 240, 240)
#define COLOR_NUMERO_NORMAL RGB(96, 96, 96)
#define COLOR_NUMERO_ACTIVO RGB(0, 92, 200)
#define COLOR_SEPARADOR     RGB(205, 205, 205)

/* Separación entre el número y el borde derecho del panel */
#define MARGEN_NUMEROS 8

/* El EDIT deja 1 px por encima de su primera línea de texto respecto a su
   área de cliente: ese píxel se compensa aquí para que el número quede
   exactamente sobre su línea (medido igual con la fuente del sistema y
   con fuentes nuevas como Consolas). */
#define AJUSTE_LINEA 1

/* Estado del panel */
static int g_primeraVisible = 1; /* primera línea a la vista (1-based)  */
static int g_lineaActual    = 0; /* línea del cursor (0-based)          */
static int g_totalLineas    = 1; /* total de líneas del documento       */
static int g_anchoPanel     = 0; /* ancho en píxeles del panel          */

/* Fuente del panel. El panel es una clase de ventana propia y DefWindowProc
   NO guarda la fuente en WM_SETFONT (solo lo hacen los controles estándar),
   así que hay que conservarla aquí para que los números usen la misma
   tipografía que el EDIT y las filas coincidan. */
static HFONT g_hFuentePanel = NULL;

/* ---------------------------------------------------------------
   ObtenerEditor: devuelve el EDIT principal que acompaña al panel
   --------------------------------------------------------------- */
static HWND ObtenerEditor(HWND hPanel)
{
    HWND hPadre = GetParent(hPanel);
    if (!hPadre) return NULL;
    return GetDlgItem(hPadre, IDC_EDIT);
}

/* ---------------------------------------------------------------
   LineasAnchoRequerido: ancho necesario según los dígitos del total
   de líneas y el ancho de un dígito con la fuente del EDIT
   --------------------------------------------------------------- */
int LineasAnchoRequerido(HWND hwndPrincipal)
{
    HWND hEdit = GetDlgItem(hwndPrincipal, IDC_EDIT);
    if (!hEdit) return 0;

    int total = (int)SendMessage(hEdit, EM_GETLINECOUNT, 0, 0);
    if (total < 1) total = 1;

    int digitos = 1;
    for (int n = total; n >= 10; n /= 10) digitos++;

    HDC hdc = GetDC(hEdit);
    if (!hdc) return digitos * 8 + MARGEN_NUMEROS * 2;

    HFONT hFuente = (HFONT)SendMessage(hEdit, WM_GETFONT, 0, 0);
    HFONT hPrevio = hFuente ? (HFONT)SelectObject(hdc, hFuente) : NULL;

    SIZE sz;
    GetTextExtentPoint32W(hdc, L"0", 1, &sz);

    if (hPrevio) SelectObject(hdc, hPrevio);
    ReleaseDC(hEdit, hdc);

    return digitos * sz.cx + MARGEN_NUMEROS * 2;
}

/* ---------------------------------------------------------------
   MargenClienteEdit: margen vertical entre el borde de la ventana del
   EDIT y su área de cliente (el EDIT tiene WS_EX_CLIENTEDGE). El texto
   empieza dentro de esa área, así que los números deben usar el mismo
   margen para que las filas coincidan.
   --------------------------------------------------------------- */
static int MargenClienteEdit(HWND hEdit)
{
    RECT rVentana;
    if (!GetWindowRect(hEdit, &rVentana)) return 0;

    POINT pt = { 0, 0 };
    if (!ClientToScreen(hEdit, &pt)) return 0;

    int margen = (int)(pt.y - rVentana.top);
    return margen > 0 ? margen : 0;
}

/* ---------------------------------------------------------------
   PintarPanel: dibuja los números de las líneas visibles
   --------------------------------------------------------------- */
static void PintarPanel(HWND hPanel)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hPanel, &ps);
    RECT rc;
    GetClientRect(hPanel, &rc);

    /* Fondo */
    HBRUSH hFondo = CreateSolidBrush(COLOR_FONDO_NUMEROS);
    FillRect(hdc, &rc, hFondo);
    DeleteObject(hFondo);

    /* Separador vertical entre los números y el texto */
    HPEN hLapiz = CreatePen(PS_SOLID, 1, COLOR_SEPARADOR);
    HPEN hLapizPrevio = (HPEN)SelectObject(hdc, hLapiz);
    MoveToEx(hdc, rc.right - 1, 0, NULL);
    LineTo(hdc, rc.right - 1, rc.bottom);
    SelectObject(hdc, hLapizPrevio);
    DeleteObject(hLapiz);

    HWND hEdit = ObtenerEditor(hPanel);
    if (!hEdit) {
        EndPaint(hPanel, &ps);
        return;
    }

    /* Misma fuente que el EDIT para que las filas coincidan exactamente */
    HFONT hFuente = g_hFuentePanel;
    HFONT hFuentePrevia = hFuente ? (HFONT)SelectObject(hdc, hFuente) : NULL;

    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    /* El alto de línea del control EDIT es tmHeight: no incluye el
       "external leading" (comprobado con Arial 12, que lo tiene = 1 y
       aun así avanza 18 px, no 19). */
    int altoLinea = tm.tmHeight;
    if (altoLinea <= 0) altoLinea = 12;

    int total = (int)SendMessage(hEdit, EM_GETLINECOUNT, 0, 0);
    if (total < 1) total = 1;

    int primera = (int)SendMessage(hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);
    if (primera < 0) primera = 0;
    int lineasVisibles = rc.bottom / altoLinea + 2;

#ifdef DEBUG_LOG
    {
        HANDLE hf = CreateFile(L"C:\\Users\\Beto\\AppData\\Local\\Temp\\opencode\\paint.log",
                               FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
        if (hf != INVALID_HANDLE_VALUE) {
            char buf[512];
            int n = wsprintfA(buf, "PAINT rc=%ldx%ld alto=%d total=%d primera=%d lineasVis=%d font=%p\n",
                              (long)rc.right, (long)rc.bottom, altoLinea, total, primera, lineasVisibles, (void*)hFuente);
            DWORD w; WriteFile(hf, buf, n, &w, NULL); CloseHandle(hf);
        }
    }
#endif

    SetBkMode(hdc, TRANSPARENT);

    /* Margen del borde del EDIT: el texto del EDIT empieza dentro de su
       área de cliente, de modo que los números deben desplazarse igual */
    int margen = MargenClienteEdit(hEdit);

#ifdef DEBUG_LOG
    {
        HANDLE hf = CreateFile(L"C:\\Users\\Beto\\AppData\\Local\\Temp\\opencode\\paint.log",
                               FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
        if (hf != INVALID_HANDLE_VALUE) {
            char buf[512];
            int n = wsprintfA(buf, "METRIC tmAscent=%ld tmDescent=%ld tmHeight=%ld extLead=%ld margen=%d\n",
                              (long)tm.tmAscent, (long)tm.tmDescent, (long)tm.tmHeight,
                              (long)tm.tmExternalLeading, margen);
            DWORD w; WriteFile(hf, buf, n, &w, NULL); CloseHandle(hf);
        }
    }
#endif

    for (int i = 0; i < lineasVisibles; i++) {
        int numero = primera + i + 1;
        if (numero > total) break;

        WCHAR texto[16];
        int len = wsprintf(texto, L"%d", numero);

        SIZE sz;
        GetTextExtentPoint32W(hdc, texto, len, &sz);

        int x = (int)(rc.right - MARGEN_NUMEROS - sz.cx);

        /* Cada línea se dibuja en la misma posición que usa el EDIT: el
           inicio de la línea n está a "margen + n * altoLinea" del borde
           del EDIT (GDI sitúa la línea base en y + tmAscent, igual que
           el control EDIT). Así los números caen exactamente sobre su
           línea de texto sea cual sea la fuente elegida.
           AJUSTE_LINEA compensa el píxel que el EDIT deja por encima de
           su primera línea. */
        int yLinea = margen + i * altoLinea + AJUSTE_LINEA;

        if (numero == g_lineaActual + 1)
            SetTextColor(hdc, COLOR_NUMERO_ACTIVO);
        else
            SetTextColor(hdc, COLOR_NUMERO_NORMAL);

        TextOutW(hdc, x, yLinea, texto, len);
    }

    if (hFuentePrevia) SelectObject(hdc, hFuentePrevia);
    EndPaint(hPanel, &ps);
}

/* ---------------------------------------------------------------
   Procedimiento de ventana del panel
   --------------------------------------------------------------- */
static LRESULT CALLBACK PanelNumerosProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:
        PintarPanel(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1; /* el fondo se pinta completo en WM_PAINT */

    case WM_SETFONT:
        /* La fuente se guarda aquí porque DefWindowProc no la conserva en
           las clases de ventana propias; sin esto el panel seguiría
           pintando con la fuente del sistema y las líneas no coincidirían
           con las del EDIT. */
        g_hFuentePanel = (HFONT)wp;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_LBUTTONDOWN:
        /* Al pulsar el panel, el foco vuelve al editor */
        SetFocus(ObtenerEditor(hwnd));
        return 0;

    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return 1;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ---------------------------------------------------------------
   CrearPanelNumeros: registra la clase y crea el control hijo
   --------------------------------------------------------------- */
HWND CrearPanelNumeros(HWND hwndPadre, HINSTANCE hInst)
{
    WNDCLASSEX wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = PanelNumerosProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = CLASE_NUMEROS;
    RegisterClassEx(&wc); /* si ya existe, se ignora el error */

    HWND hPanel = CreateWindowEx(
        0, CLASE_NUMEROS, L"",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwndPadre, (HMENU)(INT_PTR)IDC_NUMEROS_LINEA, hInst, NULL);

    if (hPanel) {
        HWND hEdit = GetDlgItem(hwndPadre, IDC_EDIT);
        if (hEdit)
            SendMessage(hPanel, WM_SETFONT,
                        (WPARAM)SendMessage(hEdit, WM_GETFONT, 0, 0), TRUE);
    }
    return hPanel;
}

/* ---------------------------------------------------------------
   LineasReflow: reacomoda el panel y el EDIT dentro de la ventana
   --------------------------------------------------------------- */
void LineasReflow(HWND hwndPrincipal)
{
    RECT rc;
    GetClientRect(hwndPrincipal, &rc);

    /* El alto disponible descuenta la barra de estado (si está visible) */
    int alto = AltoAreaEdicion(hwndPrincipal);
    if (alto <= 0) alto = rc.bottom;

    HWND hPanel = GetDlgItem(hwndPrincipal, IDC_NUMEROS_LINEA);
    HWND hEdit  = GetDlgItem(hwndPrincipal, IDC_EDIT);
    if (!hEdit) return;

#ifdef DEBUG_LOG
    {
        HANDLE hf = CreateFile(L"C:\\Users\\Beto\\AppData\\Local\\Temp\\opencode\\paint.log",
                               FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
        if (hf != INVALID_HANDLE_VALUE) {
            int total = (int)SendMessage(hEdit, EM_GETLINECOUNT, 0, 0);
            int primera = (int)SendMessage(hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);
            char buf[512];
            int n = wsprintfA(buf, "REFLOW total=%d primera=%d visible=%d\n",
                              total, primera, IsWindowVisible(hPanel) ? 1 : 0);
            DWORD w; WriteFile(hf, buf, n, &w, NULL); CloseHandle(hf);
        }
    }
#endif

    int ancho = 0;
    if (hPanel && IsWindowVisible(hPanel)) {
        ancho = LineasAnchoRequerido(hwndPrincipal);
        g_anchoPanel = ancho;
        SendMessage(hPanel, WM_SETFONT,
                    (WPARAM)SendMessage(hEdit, WM_GETFONT, 0, 0), TRUE);
        UpdateWindow(hPanel);
        MoveWindow(hPanel, 0, 0, ancho, alto, TRUE);
        InvalidateRect(hPanel, NULL, TRUE);
    }

    MoveWindow(hEdit, ancho, 0, rc.right - ancho, alto, TRUE);
}

/* ---------------------------------------------------------------
   LineasActualizar: sincroniza el panel con el EDIT (desplazamiento
   vertical, línea del cursor y número total de líneas)
   --------------------------------------------------------------- */
void LineasActualizar(HWND hEdit)
{
    if (!hEdit) return;
    HWND hPadre = GetParent(hEdit);
    HWND hPanel = GetDlgItem(hPadre, IDC_NUMEROS_LINEA);
    if (!hPanel || !IsWindowVisible(hPanel)) return;

    int primera = (int)SendMessage(hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);
    if (primera < 0) primera = 0;

    int total = (int)SendMessage(hEdit, EM_GETLINECOUNT, 0, 0);
    if (total < 1) total = 1;

    /* El cursor queda al final de la selección (EM_GETSEL: inicio en la
       palabra baja, fin en la alta). EM_LINEFROMCHAR es 0-based. */
    DWORD sel = (DWORD)SendMessage(hEdit, EM_GETSEL, 0, 0);
    int posCursor = (int)HIWORD(sel);
    int lineaActual = (int)SendMessage(hEdit, EM_LINEFROMCHAR, (WPARAM)posCursor, 0);
    if (lineaActual < 0) lineaActual = 0;

    if (primera != g_primeraVisible || lineaActual != g_lineaActual) {
        g_primeraVisible = primera;
        g_lineaActual    = lineaActual;
        InvalidateRect(hPanel, NULL, FALSE);
    }

    /* Si el documento creció y el panel ya no cabe, hay que ensancharlo */
    if (total != g_totalLineas) {
        g_totalLineas = total;
        int ancho = LineasAnchoRequerido(hPadre);
        if (ancho != g_anchoPanel) {
            g_anchoPanel = ancho;
            LineasReflow(hPadre);
            InvalidateRect(hPanel, NULL, FALSE);
        }
    }
}
