/* pestanas.c - Barra de pestañas (estilo Sublime Text)
 *
 * Es un control hijo de la ventana principal, situado en la franja superior,
 * que se dibuja a mano con GDI: cada documento abierto es una pestaña con
 * su nombre, una X de cierre y un punto cuando tiene cambios sin guardar.
 * La pestaña activa se dibuja blanca y sin borde inferior, de modo que
 * parece abrirse hacia el área de edición.
 *
 * Cuando las pestañas no caben en la franja aparecen dos flechas para
 * desplazarlas, y la rueda del ratón también las desplaza.
 */

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include <windowsx.h>
#include "app.h"
#include "resources.h"

#define CLASE_PESTANAS L"NotepadPestanas"

/* Altura de la franja de pestañas */
#define ALTO_PESTANAS 26

/* Paleta de la barra */
#define COLOR_FONDO_BARRA    RGB(238, 238, 238)
#define COLOR_PESTANA        RGB(214, 214, 214)
#define COLOR_PESTANA_ACTIVA RGB(255, 255, 255)
#define COLOR_BORDE_PESTANA  RGB(196, 196, 196)
#define COLOR_TEXTO          RGB(96, 96, 96)
#define COLOR_TEXTO_ACTIVO   RGB(24, 24, 24)
#define COLOR_MODIFICADO    RGB(206, 106, 32)

/* Medidas de cada pestaña */
#define MARGEN_IZQUIERDA 9      /* espacio antes del nombre           */
#define MARGEN_DERECHO   5      /* espacio tras la X de cierre        */
#define ANCHO_CIERRE     14     /* lado de la X de cierre             */
#define ANCHO_MINIMO     72     /* ancho mínimo de una pestaña        */
#define ANCHO_MAXIMO     220    /* ancho máximo (el nombre se corta)  */
#define RADIO            4      /* radio de las esquinas superiores   */

/* Flechas de desplazamiento */
#define ANCHO_FLECHA     17

/* ---------------------------------------------------------------
   Diseño de una pestaña: dónde está y cuál es su botón de cierre
   --------------------------------------------------------------- */
typedef struct {
    RECT rect;      /* la pestaña completa                    */
    RECT cierre;    /* la X de cierre                         */
    int  indice;    /* documento al que corresponde           */
    BOOL visible;   /* si se ve dentro de la franja           */
} PESTANA_DISENO;

static PESTANA_DISENO g_diseno[MAX_DOCUMENTOS];
static int g_cantDiseno = 0;

/* Desplazamiento horizontal de la franja (en píxeles) */
static int g_desplazamiento = 0;

/* Pestaña bajo el puntero del ratón (-1 si ninguna) */
static int g_sobreIndice = -1;

/* Flecha bajo el puntero del ratón: 0 ninguna, 1 izquierda, 2 derecha */
static int g_sobreFlecha = 0;

/* ---------------------------------------------------------------
   NombrePestana: el nombre que se escribe en la pestaña
   --------------------------------------------------------------- */
static void NombrePestana(int indice, WCHAR* buf, int cch)
{
    const WCHAR* ruta = DocsRuta(indice);
    const WCHAR* nombre = ruta;

    if (!ruta || !ruta[0]) {
        lstrcpyn(buf, L"Sin título", (size_t)cch);
        return;
    }

    /* Sólo el nombre del archivo, sin la carpeta */
    for (const WCHAR* q = ruta; *q; q++) {
        if (*q == L'\\' || *q == L'/') nombre = q + 1;
    }

    lstrcpyn(buf, nombre, (size_t)cch);
}

/* ---------------------------------------------------------------
   AnchoTotalPestanas: suma de los anchos de todas las pestañas
   --------------------------------------------------------------- */
static int AnchoTotalPestanas(HDC hdc)
{
    int total = 0;

    for (int i = 0; i < DocsCantidad(); i++) {
        WCHAR nombre[MAX_PATH];
        NombrePestana(i, nombre, MAX_PATH);

        SIZE sz;
        sz.cx = 0;
        GetTextExtentPoint32W(hdc, nombre, (int)lstrlenW(nombre), &sz);

        int ancho = sz.cx + MARGEN_IZQUIERDA + ANCHO_CIERRE + MARGEN_DERECHO;
        if (ancho < ANCHO_MINIMO) ancho = ANCHO_MINIMO;
        if (ancho > ANCHO_MAXIMO) ancho = ANCHO_MAXIMO;

        total += ancho;
    }

    return total;
}

/* ---------------------------------------------------------------
   CalcularDiseno: reparte las pestañas por la franja y guarda la
   posición de cada una para poder comprobar los clics
   --------------------------------------------------------------- */
static void CalcularDiseno(HWND hPanel, HDC hdc)
{
    RECT rc;
    GetClientRect(hPanel, &rc);

    int total = AnchoTotalPestanas(hdc);
    int flechas = (total > rc.right) ? 2 * ANCHO_FLECHA : 0;
    int disponible = rc.right - flechas;

    /* El desplazamiento se mantiene dentro de lo posible */
    int maximo = total - disponible;
    if (maximo < 0) maximo = 0;
    if (g_desplazamiento > maximo) g_desplazamiento = maximo;
    if (g_desplazamiento < 0) g_desplazamiento = 0;

    g_cantDiseno = 0;
    int x = -g_desplazamiento;

    for (int i = 0; i < DocsCantidad() && g_cantDiseno < MAX_DOCUMENTOS; i++) {
        WCHAR nombre[MAX_PATH];
        NombrePestana(i, nombre, MAX_PATH);

        SIZE sz;
        sz.cx = 0;
        GetTextExtentPoint32W(hdc, nombre, (int)lstrlenW(nombre), &sz);

        int ancho = sz.cx + MARGEN_IZQUIERDA + ANCHO_CIERRE + MARGEN_DERECHO;
        if (ancho < ANCHO_MINIMO) ancho = ANCHO_MINIMO;
        if (ancho > ANCHO_MAXIMO) ancho = ANCHO_MAXIMO;

        PESTANA_DISENO* d = &g_diseno[g_cantDiseno++];
        d->indice = i;
        d->rect.left   = x;
        d->rect.top    = 0;
        d->rect.right  = x + ancho;
        d->rect.bottom = ALTO_PESTANAS;
        d->visible = (d->rect.right > 0 && d->rect.left < disponible);

        /* La X de cierre va pegada al borde derecho de la pestaña */
        d->cierre.left   = d->rect.right - MARGEN_DERECHO - ANCHO_CIERRE;
        d->cierre.top    = (ALTO_PESTANAS - ANCHO_CIERRE) / 2;
        d->cierre.right  = d->cierre.left + ANCHO_CIERRE;
        d->cierre.bottom = d->cierre.top + ANCHO_CIERRE;

        x += ancho;
    }
}

/* ---------------------------------------------------------------
   ActualizarDiseno: recalcula la posición de las pestañas con un DC
   propio, para que los clics y el ratón funcionen incluso antes de
   que se haya pintado la franja
   --------------------------------------------------------------- */
static void ActualizarDiseno(HWND hPanel)
{
    HDC hdc = GetDC(hPanel);
    if (!hdc) return;

    HFONT hFuente = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT hPrevia = hFuente ? (HFONT)SelectObject(hdc, hFuente) : NULL;

    CalcularDiseno(hPanel, hdc);

    if (hPrevia) SelectObject(hdc, hPrevia);
    ReleaseDC(hPanel, hdc);
}

/* ---------------------------------------------------------------
   RegionPestana: región con las esquinas superiores redondeadas y
   las inferiores rectas, que es la forma de las pestañas
   --------------------------------------------------------------- */
static HRGN RegionPestana(const RECT* r)
{
    HRGN region = CreateRoundRectRgn(r->left, r->top, r->right, r->bottom,
                                     RADIO * 2, RADIO * 2);
    if (!region) return NULL;

    /* Se quitan las dos esquinas inferiores para que la pestaña se
       apoye squarely en el área de edición */
    HRGN esquina = CreateRectRgn(r->left, r->bottom - RADIO,
                                 r->left + RADIO, r->bottom);
    if (esquina) {
        CombineRgn(region, region, esquina, RGN_DIFF);
        DeleteObject(esquina);
    }

    esquina = CreateRectRgn(r->right - RADIO, r->bottom - RADIO,
                            r->right, r->bottom);
    if (esquina) {
        CombineRgn(region, region, esquina, RGN_DIFF);
        DeleteObject(esquina);
    }

    return region;
}

/* ---------------------------------------------------------------
   DibujarCierre: dibuja la X de cierre o, si el documento tiene
   cambios sin guardar, el punto que lo indica
   --------------------------------------------------------------- */
static void DibujarCierre(HDC hdc, const RECT* r, BOOL modificado)
{
    if (modificado) {
        /* Punto relleno en el color de los cambios sin guardar */
        int cx = (r->left + r->right) / 2;
        int cy = (r->top + r->bottom) / 2;
        int radio = 3;

        HBRUSH hBrocha = CreateSolidBrush(COLOR_MODIFICADO);
        HPEN hLapiz = CreatePen(PS_NULL, 0, 0);
        HPEN hLapizPrevio = (HPEN)SelectObject(hdc, hLapiz);
        HBRUSH hBrochaPrevia = (HBRUSH)SelectObject(hdc, hBrocha);

        Ellipse(hdc, cx - radio, cy - radio, cx + radio, cy + radio);

        SelectObject(hdc, hBrochaPrevia);
        SelectObject(hdc, hLapizPrevio);
        DeleteObject(hBrocha);
        DeleteObject(hLapiz);
        return;
    }

    /* X: dos diagonales dentro del recuadro */
    int margen = 4;
    HPEN hLapiz = CreatePen(PS_SOLID, 1, RGB(110, 110, 110));
    HPEN hLapizPrevio = (HPEN)SelectObject(hdc, hLapiz);

    MoveToEx(hdc, r->left + margen, r->top + margen, NULL);
    LineTo(hdc, r->right - margen, r->bottom - margen);
    MoveToEx(hdc, r->right - margen, r->top + margen, NULL);
    LineTo(hdc, r->left + margen, r->bottom - margen);

    SelectObject(hdc, hLapizPrevio);
    DeleteObject(hLapiz);
}

/* ---------------------------------------------------------------
   PintarPestana: dibuja una pestaña concreta
   --------------------------------------------------------------- */
static void PintarPestana(HDC hdc, const PESTANA_DISENO* d, BOOL activa, BOOL sobre)
{
    BOOL modificado = DocsModificado(d->indice);

    COLORREF fondo = activa ? COLOR_PESTANA_ACTIVA : COLOR_PESTANA;
    COLORREF tinta = activa ? COLOR_TEXTO_ACTIVO : COLOR_TEXTO;

    /* Relleno y contorno: se usa la misma región para los dos, así el
       borde encaja exactamente con la forma de la pestaña */
    HRGN region = RegionPestana(&d->rect);
    if (region) {
        HBRUSH hBrocha = CreateSolidBrush(fondo);
        FillRgn(hdc, region, hBrocha);
        DeleteObject(hBrocha);

        HBRUSH hBorde = CreateSolidBrush(COLOR_BORDE_PESTANA);
        FrameRgn(hdc, region, hBorde, 1, 1);
        DeleteObject(hBorde);

        DeleteObject(region);
    }

    /* El borde inferior de la pestaña activa se borra con el fondo del
       editor, para que parezca que se abre hacia el área de texto
       (el marco de la región ocupa dos píxeles) */
    if (activa) {
        RECT rAbajo;
        rAbajo.left   = d->rect.left + RADIO;
        rAbajo.right  = d->rect.right - RADIO;
        rAbajo.top    = d->rect.bottom - 2;
        rAbajo.bottom = d->rect.bottom;

        HBRUSH hBlanco = CreateSolidBrush(COLOR_PESTANA_ACTIVA);
        FillRect(hdc, &rAbajo, hBlanco);
        DeleteObject(hBlanco);
    }

    /* Nombre del documento */
    WCHAR nombre[MAX_PATH];
    NombrePestana(d->indice, nombre, MAX_PATH);

    RECT rcTexto;
    rcTexto.left   = d->rect.left + MARGEN_IZQUIERDA;
    rcTexto.top    = d->rect.top;
    rcTexto.right  = d->cierre.left - 2;
    rcTexto.bottom = d->rect.bottom;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, tinta);
    DrawTextW(hdc, nombre, -1, &rcTexto,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    /* Cierre: la X se ve en la pestaña activa y en la que está bajo el
       ratón; si el documento está modificado se dibuja el punto */
    if (modificado || activa || sobre)
        DibujarCierre(hdc, &d->cierre, modificado);
}

/* ---------------------------------------------------------------
   PintarFlechas: dibuja las flechas de desplazamiento
   --------------------------------------------------------------- */
static void PintarFlechas(HDC hdc, RECT* rc)
{
    int izquierda = rc->right - 2 * ANCHO_FLECHA;
    int medio = rc->bottom / 2;

    for (int lado = 1; lado <= 2; lado++) {
        /* La flecha sobre la que está el ratón se dibuja más oscura */
        COLORREF color = (g_sobreFlecha == lado) ? RGB(60, 60, 60) : RGB(120, 120, 120);

        HPEN hLapiz = CreatePen(PS_SOLID, 1, color);
        HPEN hPrevio = (HPEN)SelectObject(hdc, hLapiz);

        if (lado == 1) {
            MoveToEx(hdc, izquierda + 11, medio - 4, NULL);
            LineTo(hdc, izquierda + 6, medio);
            LineTo(hdc, izquierda + 11, medio + 4);
        } else {
            int derecha = rc->right - ANCHO_FLECHA;
            MoveToEx(hdc, derecha + 6, medio - 4, NULL);
            LineTo(hdc, derecha + 11, medio);
            LineTo(hdc, derecha + 6, medio + 4);
        }

        SelectObject(hdc, hPrevio);
        DeleteObject(hLapiz);
    }
}

/* ---------------------------------------------------------------
   PintarPestanas: dibuja toda la franja
   --------------------------------------------------------------- */
static void PintarPestanas(HWND hPanel)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hPanel, &ps);

    RECT rc;
    GetClientRect(hPanel, &rc);

    /* Fondo de la franja */
    HBRUSH hFondo = CreateSolidBrush(COLOR_FONDO_BARRA);
    FillRect(hdc, &rc, hFondo);
    DeleteObject(hFondo);

    HFONT hFuente = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT hPrevia = hFuente ? (HFONT)SelectObject(hdc, hFuente) : NULL;

    CalcularDiseno(hPanel, hdc);

    int total = AnchoTotalPestanas(hdc);
    int hayFlechas = (total > rc.right);

    if (hayFlechas) {
        /* Las flechas de desplazamiento se dibujan sobre un fondo algo
           más claro para que se separen de las pestañas */
        RECT rcFlechas = rc;
        rcFlechas.left = rc.right - 2 * ANCHO_FLECHA;

        HBRUSH hBrocha = CreateSolidBrush(RGB(228, 228, 228));
        FillRect(hdc, &rcFlechas, hBrocha);
        DeleteObject(hBrocha);

        PintarFlechas(hdc, &rc);
    }

    /* Las pestañas no pueden invadir la zona reservada a las flechas */
    RECT rcRecorteAnterior;
    GetClipBox(hdc, &rcRecorteAnterior);

    if (hayFlechas) {
        RECT rcZona = rc;
        rcZona.right = rc.right - 2 * ANCHO_FLECHA;
        IntersectClipRect(hdc, rcZona.left, rcZona.top, rcZona.right, rcZona.bottom);
    }

    int activo = DocsActivo();
    for (int i = 0; i < g_cantDiseno; i++) {
        const PESTANA_DISENO* d = &g_diseno[i];
        if (!d->visible) continue;

        PintarPestana(hdc, d, d->indice == activo, d->indice == g_sobreIndice);
    }

    if (hayFlechas) {
        IntersectClipRect(hdc, rcRecorteAnterior.left, rcRecorteAnterior.top,
                          rcRecorteAnterior.right, rcRecorteAnterior.bottom);
    }

    if (hPrevia) SelectObject(hdc, hPrevia);
    EndPaint(hPanel, &ps);
}

/* ---------------------------------------------------------------
   DesplazarPestanas: mueve la franja y repinta
   --------------------------------------------------------------- */
static void DesplazarPestanas(HWND hPanel, int delta)
{
    g_desplazamiento += delta;
    if (g_desplazamiento < 0) g_desplazamiento = 0;

    InvalidateRect(hPanel, NULL, FALSE);
}

/* ---------------------------------------------------------------
   BuscarPestana: devuelve el índice de la pestaña bajo el punto
   (-1 si no hay ninguna)
   --------------------------------------------------------------- */
static int BuscarPestana(HWND hPanel, int x, int y)
{
    RECT rc;
    GetClientRect(hPanel, &rc);

    if (y < 0 || y >= rc.bottom) return -1;

    for (int i = 0; i < g_cantDiseno; i++) {
        const PESTANA_DISENO* d = &g_diseno[i];
        if (!d->visible) continue;
        if (x >= d->rect.left && x < d->rect.right) return d->indice;
    }
    return -1;
}

/* ---------------------------------------------------------------
   BotonCierre: comprueba si el punto está sobre la X de una pestaña
   (devuelve el índice del documento, o -1)
   --------------------------------------------------------------- */
static int BotonCierre(int x, int y)
{
    for (int i = 0; i < g_cantDiseno; i++) {
        const PESTANA_DISENO* d = &g_diseno[i];
        if (!d->visible) continue;
        if (x >= d->cierre.left && x < d->cierre.right &&
            y >= d->cierre.top && y < d->cierre.bottom)
            return d->indice;
    }
    return -1;
}

/* ---------------------------------------------------------------
   RatonSobreFlecha: devuelve 1 o 2 si el punto está sobre una de
   las flechas de desplazamiento, 0 si no
   --------------------------------------------------------------- */
static int RatonSobreFlecha(HWND hPanel, int x, int y)
{
    RECT rc;
    GetClientRect(hPanel, &rc);

    HDC hdc = GetDC(hPanel);
    if (!hdc) return 0;

    HFONT hFuente = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT hPrevia = hFuente ? (HFONT)SelectObject(hdc, hFuente) : NULL;

    int total = AnchoTotalPestanas(hdc);
    if (hPrevia) SelectObject(hdc, hPrevia);
    ReleaseDC(hPanel, hdc);

    if (total <= rc.right) return 0;
    if (y < 0 || y >= rc.bottom) return 0;

    if (x >= rc.right - 2 * ANCHO_FLECHA && x < rc.right - ANCHO_FLECHA) return 1;
    if (x >= rc.right - ANCHO_FLECHA && x < rc.right) return 2;
    return 0;
}

/* ---------------------------------------------------------------
   Procedimiento de ventana de la barra de pestañas
   --------------------------------------------------------------- */
static LRESULT CALLBACK PestanasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:
        PintarPestanas(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1; /* el fondo se pinta completo en WM_PAINT */

    case WM_SIZE:
        g_desplazamiento = 0;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lp);
        int y = GET_Y_LPARAM(lp);

        ActualizarDiseno(hwnd);

        int flecha = RatonSobreFlecha(hwnd, x, y);
        if (flecha == 1) { DesplazarPestanas(hwnd, -60); return 0; }
        if (flecha == 2) { DesplazarPestanas(hwnd, 60);  return 0; }

        /* La X de la pestaña cierra ese documento */
        int indice = BotonCierre(x, y);
        if (indice >= 0) {
            HWND hPrincipal = GetParent(hwnd);
            DocsCerrar(hPrincipal, indice);
            return 0;
        }

        /* Clic en el cuerpo: activar esa pestaña */
        indice = BuscarPestana(hwnd, x, y);
        if (indice >= 0) {
            DocsSeleccionar(indice);
            SetFocus(DocsEditActivo());
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lp);
        int y = GET_Y_LPARAM(lp);

        ActualizarDiseno(hwnd);

        int sobre = BuscarPestana(hwnd, x, y);
        int flecha = RatonSobreFlecha(hwnd, x, y);

        if (sobre != g_sobreIndice || flecha != g_sobreFlecha) {
            g_sobreIndice = sobre;
            g_sobreFlecha = flecha;
            InvalidateRect(hwnd, NULL, FALSE);
        }

        /* Hay que avisar cuando el ratón sale del control para poder
           quitar la X de la pestaña señalada */
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        g_sobreIndice = -1;
        g_sobreFlecha = 0;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_MOUSEWHEEL: {
        DesplazarPestanas(hwnd, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -40 : 40);
        return 0;
    }

    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return 1;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ---------------------------------------------------------------
   CrearPestanas: registra la clase y crea el control hijo
   --------------------------------------------------------------- */
HWND CrearPestanas(HWND hwndPadre, HINSTANCE hInst)
{
    WNDCLASSEX wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = PestanasProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = CLASE_PESTANAS;
    RegisterClassEx(&wc); /* si ya existe, se ignora el error */

    HWND hPanel = CreateWindowEx(
        0, CLASE_PESTANAS, L"",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, ALTO_PESTANAS, hwndPadre, (HMENU)(INT_PTR)IDC_PESTANAS, hInst, NULL);

    return hPanel;
}

/* ---------------------------------------------------------------
   PestanasReflow: coloca la franja de pestañas arriba del todo
   --------------------------------------------------------------- */
void PestanasReflow(HWND hwndPrincipal)
{
    HWND hPanel = GetDlgItem(hwndPrincipal, IDC_PESTANAS);
    if (!hPanel) return;

    RECT rc;
    GetClientRect(hwndPrincipal, &rc);

    MoveWindow(hPanel, 0, 0, rc.right, ALTO_PESTANAS, TRUE);
}

/* ---------------------------------------------------------------
   PestanasAlto: alto que ocupa la franja de pestañas (0 si no está)
   --------------------------------------------------------------- */
int PestanasAlto(HWND hwndPrincipal)
{
    HWND hPanel = GetDlgItem(hwndPrincipal, IDC_PESTANAS);
    if (!hPanel || !IsWindowVisible(hPanel)) return 0;
    return ALTO_PESTANAS;
}

/* ---------------------------------------------------------------
   PestanasActualizar: repinta la franja (nombres, marca de cambios)
   --------------------------------------------------------------- */
void PestanasActualizar(HWND hwndPrincipal)
{
    HWND hPanel = GetDlgItem(hwndPrincipal, IDC_PESTANAS);
    if (hPanel) InvalidateRect(hPanel, NULL, FALSE);
}
