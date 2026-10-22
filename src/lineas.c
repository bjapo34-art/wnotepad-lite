/* lineas.c - Líneas lógicas del documento
 *
 * El control EDIT llama "líneas" a las filas que muestra en pantalla: con
 * el ajuste de línea activo, una línea larga ocupa varias y EM_GETLINECOUNT,
 * EM_LINEINDEX y EM_LINEFROMCHAR trabajan con esas líneas visuales.
 *
 * Para el usuario una línea sólo termina con un salto de línea real. Este
 * módulo traduce entre las dos numeraciones usando el texto del documento,
 * que se guarda en caché y se invalida cuando el EDIT avisa de que su
 * contenido ha cambiado (ver EsMensajeDeContenido en main.c).
 *
 * Todas las funciones se ejecutan en el hilo de la interfaz y sólo guardan
 * el texto del último EDIT consultado (en la práctica, el activo).
 */

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include "app.h"

/* Texto del documento en caché */
static HWND    g_hEdit       = NULL;
static WCHAR*  g_texto       = NULL;
static int     g_capacidad   = 0;
static int     g_largo       = 0;
static BOOL    g_valido      = FALSE;
static int     g_totalLineas = 1; /* líneas lógicas (saltos de línea) */

/* Caché de avance para LineasLogicaDePos: posición, número de línea y
   comienzo de esa línea. Así, al escribir o mover el cursor hacia
   delante, no hay que recorrer el texto desde el principio. */
static int g_pos    = 0;
static int g_linea  = 1;
static int g_inicio = 0;

/* ---------------------------------------------------------------
   LiberarTexto: olvida el texto y todas las cachés derivadas
   --------------------------------------------------------------- */
static void LiberarTexto(void)
{
    if (g_texto) {
        LocalFree(g_texto);
        g_texto = NULL;
    }
    g_capacidad   = 0;
    g_largo       = 0;
    g_valido      = FALSE;
    g_totalLineas = 1;
    g_pos         = 0;
    g_linea       = 1;
    g_inicio      = 0;
}

/* ---------------------------------------------------------------
   AsegurarTexto: carga el texto del EDIT si la caché no es válida
   --------------------------------------------------------------- */
static BOOL AsegurarTexto(HWND hEdit)
{
    if (hEdit != g_hEdit) {
        LiberarTexto();
        g_hEdit = hEdit;
    }

    if (g_valido) return TRUE;
    if (!hEdit || !IsWindow(hEdit)) return FALSE;

    int largo = GetWindowTextLengthW(hEdit);
    if (largo < 0) largo = 0;

    if (largo + 1 > g_capacidad) {
        int nueva = largo + 1;
        if (nueva < 256) nueva = 256;

        WCHAR* nuevo = (WCHAR*)LocalAlloc(LPTR,
                                          (SIZE_T)nueva * sizeof(WCHAR));
        if (!nuevo) return FALSE;

        if (g_texto) LocalFree(g_texto);
        g_texto     = nuevo;
        g_capacidad = nueva;
    }

    g_texto[0] = L'\0';
    if (largo > 0)
        GetWindowTextW(hEdit, g_texto, largo + 1);
    g_largo = largo;

    /* Las líneas lógicas son los saltos de línea reales más uno */
    int total = 1;
    for (int i = 0; i < largo; i++) {
        if (g_texto[i] == L'\n') total++;
    }
    g_totalLineas = total;

    g_pos    = 0;
    g_linea  = 1;
    g_inicio = 0;
    g_valido = TRUE;
    return TRUE;
}

/* ---------------------------------------------------------------
   LineasInvalidarTexto: marca la caché como obsoleta. Se llama al
   recibir mensajes que pueden modificar el contenido del EDIT.
   --------------------------------------------------------------- */
void LineasInvalidarTexto(HWND hEdit)
{
    /* Si el EDIT no es el cacheado no hay nada que invalidar: al
       consultarlo, AsegurarTexto detectará el cambio de control. */
    if (hEdit == g_hEdit)
        g_valido = FALSE;
}

/* ---------------------------------------------------------------
   LineasTotalLogicas: número de líneas lógicas del documento
   --------------------------------------------------------------- */
int LineasTotalLogicas(HWND hEdit)
{
    if (!AsegurarTexto(hEdit)) return 1;
    return g_totalLineas;
}

/* ---------------------------------------------------------------
   LineasLogicaDePos: línea lógica (1-based) que contiene la posición
   de carácter indicada. Si 'inicio' no es NULL, recibe el desplazamiento
   donde empieza esa línea (para calcular la columna).
   --------------------------------------------------------------- */
int LineasLogicaDePos(HWND hEdit, int pos, int* inicio)
{
    if (!AsegurarTexto(hEdit)) {
        if (inicio) *inicio = 0;
        return 1;
    }

    if (pos < 0) pos = 0;
    if (pos > g_largo) pos = g_largo;

    /* Si se consulta una posición anterior a la cacheada, hay que
       empezar de nuevo desde el principio. */
    if (pos < g_pos) {
        g_pos    = 0;
        g_linea  = 1;
        g_inicio = 0;
    }

    while (g_pos < pos) {
        if (g_texto[g_pos] == L'\n') {
            g_linea++;
            g_inicio = g_pos + 1;
        }
        g_pos++;
    }

    if (inicio) *inicio = g_inicio;
    return g_linea;
}

/* ---------------------------------------------------------------
   LineasPosDeLogica: desplazamiento de carácter donde empieza la línea
   lógica indicada (1-based). Si se pide una línea posterior al final,
   devuelve el final del texto.
   --------------------------------------------------------------- */
int LineasPosDeLogica(HWND hEdit, int logica)
{
    if (!AsegurarTexto(hEdit)) return 0;
    if (logica <= 1) return 0;

    int linea = 1;
    for (int i = 0; i < g_largo; i++) {
        if (g_texto[i] == L'\n') {
            linea++;
            if (linea == logica)
                return i + 1;
        }
    }
    return g_largo;
}

/* ---------------------------------------------------------------
   LineasNumerosVisibles: rellena 'numeros' con el número de línea lógica
   que corresponde a cada fila visible del EDIT (empezando en la línea
   visual 'primera'), o 0 si la fila es una continuación por ajuste de
   línea. Devuelve cuántas filas se han rellenado.

   Se usa en el panel para no repetir el número de una misma línea
   lógica en todas las filas que ocupa al ajustarse.
   --------------------------------------------------------------- */
int LineasNumerosVisibles(HWND hEdit, int primera, int cuantas, int* numeros)
{
    if (!numeros || cuantas <= 0) return 0;
    if (!AsegurarTexto(hEdit)) return 0;

    int totalVisual = (int)SendMessage(hEdit, EM_GETLINECOUNT, 0, 0);
    if (totalVisual < 1) totalVisual = 1;
    if (primera < 0) primera = 0;

    int hechas = 0;
    int logica = 0;

    for (int i = 0; i < cuantas; i++) {
        int visual = primera + i;
        if (visual >= totalVisual) break;

        int off = (int)SendMessage(hEdit, EM_LINEINDEX, (WPARAM)visual, 0);
        if (off < 0 || off > g_largo) break;

        /* Una fila empieza una línea lógica si es la primera del texto
           o si el carácter anterior es un salto de línea. Las filas que
           sólo son continuación del ajuste no empiezan línea. */
        BOOL esInicio = (off == 0) ||
                        (off > 0 && g_texto[off - 1] == L'\n');

        if (i == 0)
            logica = LineasLogicaDePos(hEdit, off, NULL);
        else if (esInicio)
            logica++;

        numeros[i] = esInicio ? logica : 0;
        hechas++;
    }

    return hechas;
}
