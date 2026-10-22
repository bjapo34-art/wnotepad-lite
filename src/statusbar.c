/* statusbar.c - Barra de estado (estilo Bloc de notas de Windows)
 *
 * Muestra información relevante del documento abierto y de la posición
 * del cursor:
 *
 *   | Lín. 12, Col. 5 | 320 líneas · 4.578 caracteres | documento.txt (1,2 KB) |
 *
 * La barra de estado es un control hijo de la ventana principal, por lo que
 * el área de edición debe cederle el espacio inferior. Para eso se expone
 * AltoAreaEdicion(), que usan linenumbers.c y window.c al reacomodar los
 * controles.
 */

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include <commctrl.h>
#include "app.h"
#include "resources.h"

/* Altura por defecto de la barra de estado (en píxeles) */
#define ALTO_BARRA_DEFECTO 24

/* Índices de las partes de la barra de estado */
enum {
    PARTE_POSICION   = 0, /* Lín. X, Col. Y                       */
    PARTE_DATOS      = 1, /* N líneas · N caracteres              */
    PARTE_CODIFIC    = 2, /* UTF-8 (con BOM)                      */
    PARTE_ARCHIVO    = 3, /* nombre y tamaño del archivo abierto  */
    TOTAL_PARTES     = 4
};

/* Ancho reservado a la parte de posición (Lín. / Col.) */
#define ANCHO_PARTE_POSICION 130

/* Margen mínimo entre la parte de datos y la parte del archivo */
#define MARGEN_PARTES 20

/* ---------------------------------------------------------------
   AltoBarraEstado: altura real de la barra de estado
   --------------------------------------------------------------- */
static int AltoBarraEstado(HWND hStatus)
{
    RECT rc;
    if (GetWindowRect(hStatus, &rc)) {
        int alto = rc.bottom - rc.top;
        if (alto > 0) return alto;
    }
    return ALTO_BARRA_DEFECTO;
}

/* ---------------------------------------------------------------
   AltoAreaEdicion: alto disponible para el editor y el panel de
   números (área de cliente menos la barra de estado)
   --------------------------------------------------------------- */
int AltoAreaEdicion(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    HWND hStatus = GetDlgItem(hwnd, IDC_STATUSBAR);
    if (!hStatus || !IsWindowVisible(hStatus)) return rc.bottom;

    int disponible = rc.bottom - AltoBarraEstado(hStatus);
    if (disponible < 0) disponible = 0;
    return disponible;
}

/* ---------------------------------------------------------------
   NombreArchivo: extrae el nombre del archivo de una ruta completa
   --------------------------------------------------------------- */
static const WCHAR* NombreArchivo(const WCHAR* ruta)
{
    const WCHAR* nombre = ruta;
    for (const WCHAR* q = ruta; *q; q++) {
        if (*q == L'\\' || *q == L'/') nombre = q + 1;
    }
    return nombre;
}

/* ---------------------------------------------------------------
   FormatoTamano: escribe un tamaño en bytes con unidades legibles
   --------------------------------------------------------------- */
static void FormatoTamano(WCHAR* buf, DWORD bytes)
{
    if (bytes < 1024) {
        wsprintf(buf, L"%lu bytes", (unsigned long)bytes);
    } else if (bytes < 1024 * 1024) {
        /* KiB con un decimal, separador decimal coma (estilo español) */
        DWORD entero  = bytes / 1024;
        DWORD decimal = ((bytes % 1024) * 10) / 1024;
        wsprintf(buf, L"%lu,%lu KB", (unsigned long)entero, (unsigned long)decimal);
    } else {
        DWORD entero  = bytes / (1024 * 1024);
        DWORD decimal = (((bytes % (1024 * 1024)) * 10) / (1024 * 1024));
        wsprintf(buf, L"%lu,%lu MB", (unsigned long)entero, (unsigned long)decimal);
    }
}

/* ---------------------------------------------------------------
   TextoArchivo: texto de la parte derecha (datos del archivo abierto)
   --------------------------------------------------------------- */
static void TextoArchivo(WCHAR* buf, int cch)
{
    const WCHAR* ruta = ObtenerRutaActual();
    if (ruta && ruta[0]) {
        WCHAR tam[64];
        WCHAR completo[MAX_PATH + 96];
        FormatoTamano(tam, ObtenerTamanoArchivo());
        wsprintf(completo, L"%s  (%s)", NombreArchivo(ruta), tam);
        lstrcpyn(buf, completo, cch);
    } else {
        lstrcpyn(buf, L"Sin título", cch);
    }
}

/* ---------------------------------------------------------------
   TextoCodificacion: texto de la parte de codificación (UTF-8 con BOM,
   UTF-16, ANSI...), que se detecta al abrir el archivo
   --------------------------------------------------------------- */
static void TextoCodificacion(WCHAR* buf, int cch)
{
    lstrcpyn(buf, NombreCodificacion(CodificacionActual()), cch);
}

/* ---------------------------------------------------------------
   AnchoTexto: mide un texto con la fuente de la barra de estado
   --------------------------------------------------------------- */
static int AnchoTexto(HWND hStatus, const WCHAR* texto)
{
    if (!texto || !texto[0]) return 0;

    HDC hdc = GetDC(hStatus);
    if (!hdc) return 0;

    HFONT hFuente = (HFONT)SendMessage(hStatus, WM_GETFONT, 0, 0);
    HFONT hPrevia = hFuente ? (HFONT)SelectObject(hdc, hFuente) : NULL;

    SIZE sz;
    sz.cx = 0;
    GetTextExtentPoint32W(hdc, texto, (int)lstrlenW(texto), &sz);

    if (hPrevia) SelectObject(hdc, hPrevia);
    ReleaseDC(hStatus, hdc);

    return sz.cx;
}

/* ---------------------------------------------------------------
   FijarPartes: reparte las partes de la barra.
   La última parte (datos del archivo) se alinea a la derecha midiendo
   el ancho de su texto, de modo que quede pegada al borde derecho.
   La parte de la codificación se coloca justo a su izquierda. Devuelve
   FALSE si no hay sitio para mostrar la codificación.
   --------------------------------------------------------------- */
static BOOL FijarPartes(HWND hStatus, const WCHAR* textoCodificacion, const WCHAR* textoArchivo)
{
    RECT rc;
    GetClientRect(hStatus, &rc);
    int ancho = rc.right;
    if (ancho <= 0) return FALSE;

    /* Borde izquierdo de la parte derecha = ancho total - ancho del texto */
    int anchoTexto = AnchoTexto(hStatus, textoArchivo) + MARGEN_PARTES;
    int inicioDerecha = ancho - anchoTexto;

    /* No invadir las partes de la izquierda */
    int minimo = ANCHO_PARTE_POSICION + 160;
    if (inicioDerecha < minimo) inicioDerecha = minimo;
    if (inicioDerecha > ancho) inicioDerecha = ancho;

    /* La codificación va a la izquierda del archivo, con un margen entre
       ambas. Si no cabe, la parte se deja vacía. */
    int anchoCod = AnchoTexto(hStatus, textoCodificacion);
    int inicioCod = inicioDerecha - MARGEN_PARTES - anchoCod;
    BOOL haySitio = (inicioCod > minimo);
    if (!haySitio) inicioCod = minimo;

    int partes[TOTAL_PARTES + 1];
    partes[0] = ANCHO_PARTE_POSICION;   /* borde derecho de la parte 0 */
    partes[1] = inicioCod;              /* borde derecho de la parte 1 */
    partes[2] = inicioDerecha;          /* borde derecho de la parte 2 */
    partes[3] = ancho;                  /* borde derecho de la parte 3 */
    partes[4] = ancho;                  /* ancho total (por compatibilidad) */

    SendMessage(hStatus, SB_SETPARTS, TOTAL_PARTES, (LPARAM)partes);
    return haySitio;
}

/* ---------------------------------------------------------------
   CrearBarraEstado: crea el control de barra de estado
   --------------------------------------------------------------- */
HWND CrearBarraEstado(HWND hwndPadre, HINSTANCE hInst)
{
    UNREFERENCED_PARAMETER(hInst);

    HWND hStatus = CreateWindowEx(
        0, STATUSCLASSNAME, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 200, ALTO_BARRA_DEFECTO,
        hwndPadre, (HMENU)(INT_PTR)IDC_STATUSBAR, NULL, NULL);

    if (hStatus) {
        int partes[TOTAL_PARTES + 1] = { ANCHO_PARTE_POSICION, 300, 400, 400 };
        SendMessage(hStatus, SB_SETPARTS, TOTAL_PARTES, (LPARAM)partes);
    }
    return hStatus;
}

/* ---------------------------------------------------------------
   ReflowBarraEstado: coloca la barra al fondo y reajusta las partes
   --------------------------------------------------------------- */
void ReflowBarraEstado(HWND hwnd)
{
    HWND hStatus = GetDlgItem(hwnd, IDC_STATUSBAR);
    if (!hStatus) return;

    RECT rc;
    GetClientRect(hwnd, &rc);

    if (!IsWindowVisible(hStatus)) return;

    int alto = AltoBarraEstado(hStatus);

    MoveWindow(hStatus, 0, rc.bottom - alto, rc.right, alto, TRUE);

    /* Recalcular el reparto de partes según el texto del archivo */
    WCHAR textoDerecha[512];
    TextoArchivo(textoDerecha, 512);
    WCHAR textoCod[64];
    TextoCodificacion(textoCod, 64);
    FijarPartes(hStatus, textoCod, textoDerecha);

    /* Repintar el texto, porque SB_SETPARTS puede vaciar las partes */
    ActualizarBarraEstado(hwnd);
}

/* ---------------------------------------------------------------
   ActualizarBarraEstado: refresca línea/columna, líneas, caracteres
   y nombre del archivo abierto
   --------------------------------------------------------------- */
void ActualizarBarraEstado(HWND hwnd)
{
    HWND hStatus = GetDlgItem(hwnd, IDC_STATUSBAR);
    HWND hEdit   = DocsEditActivo();
    if (!hStatus || !hEdit || !IsWindowVisible(hStatus)) return;

    /* Posición del cursor: el cursor queda al final de la selección. Se
       usa la variante con punteros porque la empaquetada pierde los bits
       altos en documentos de más de 65.535 caracteres. */
    DWORD inicio = 0, fin = 0;
    SendMessage(hEdit, EM_GETSEL, (WPARAM)&inicio, (LPARAM)&fin);
    int cursor = (int)(fin >= inicio ? fin : inicio);

    /* Línea y columna LÓGICAS: con el ajuste de línea activo, las filas
       en las que el EDIT parte una línea larga no cuentan como líneas. */
    int inicioLinea = 0;
    int linea0 = LineasLogicaDePos(hEdit, cursor, &inicioLinea) - 1;
    if (linea0 < 0) linea0 = 0;
    int col0 = cursor - inicioLinea;

    /* Número de líneas lógicas y de caracteres del documento */
    int totalLineas = LineasTotalLogicas(hEdit);
    if (totalLineas < 1) totalLineas = 1;
    int totalChars = GetWindowTextLength(hEdit);

    /* Datos del archivo abierto: nombre y tamaño en disco */
    WCHAR bufArchivo[512];
    TextoArchivo(bufArchivo, 512);

    /* Codificación con la que se guardará el documento */
    WCHAR bufCod[64];
    TextoCodificacion(bufCod, 64);

    /* Primero se reparten las partes (la última se alinea a la derecha
       midiendo el texto) y después se escriben los textos. */
    BOOL conCod = FijarPartes(hStatus, bufCod, bufArchivo);

    WCHAR bufPos[64], bufDatos[128];

    wsprintf(bufPos, L"Lín. %d, Col. %d", linea0 + 1, col0 + 1);
    SendMessage(hStatus, SB_SETTEXT, PARTE_POSICION, (LPARAM)bufPos);

    wsprintf(bufDatos, L"%d %s · %d %s",
             totalLineas, totalLineas == 1 ? L"línea" : L"líneas",
             totalChars, totalChars == 1 ? L"carácter" : L"caracteres");
    SendMessage(hStatus, SB_SETTEXT, PARTE_DATOS, (LPARAM)bufDatos);

    SendMessage(hStatus, SB_SETTEXT, PARTE_CODIFIC, (LPARAM)(conCod ? bufCod : L""));

    SendMessage(hStatus, SB_SETTEXT, PARTE_ARCHIVO, (LPARAM)bufArchivo);
}
