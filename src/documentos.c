/* documentos.c - Modelo de documentos del editor (uno por pestaña)
 *
 * Cada documento abierto tiene su propio control EDIT, de modo que conserva
 * su pila de deshacer, su selección y su posición de desplazamiento, y su
 * propia ficha en la barra de pestañas. Aquí se guarda la lista de
 * documentos, cuál de ellos está activo y los datos que pertenecen a cada
 * uno (ruta del archivo y codificación con la que se guarda).
 *
 * Los EDIT se crean sin WS_VISIBLE: sólo se ve el del documento activo.
 * Todos se colocan en la misma zona de la ventana, superpuestos, y el
 * panel de números de línea y la barra de estado trabajan siempre con el
 * documento activo.
 */

#define WIN32_LEAN_AND_MEAN
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

/* ---------------------------------------------------------------
   Documento: un archivo abierto (o un documento nuevo sin guardar)
   --------------------------------------------------------------- */
typedef struct {
    HWND         hEdit;          /* control EDIT propio del documento  */
    WCHAR        ruta[MAX_PATH]; /* ruta del archivo (vacía si es nuevo) */
    CODIFICACION cod;            /* codificación con la que se guarda  */
    BOOL         modificado;     /* ¿tiene cambios sin guardar?        */
} DOCUMENTO;

static DOCUMENTO g_documentos[MAX_DOCUMENTOS];
static int g_cantidad = 0;   /* número de documentos abiertos */
static int g_activo   = -1;  /* índice del documento activo  */

/* Preferencia de la ventana: ajuste de línea activado. El control EDIT
   decide si ajusta o no en el momento de crearse (según ES_AUTOHSCROLL),
   así que cambiarla obliga a recrear los EDIT (ver DocsEstablecerAjuste). */
static BOOL g_ajusteLinea = TRUE;

/* ---------------------------------------------------------------
   Acceso a los datos de la lista
   --------------------------------------------------------------- */
int DocsCantidad(void)
{
    return g_cantidad;
}

int DocsActivo(void)
{
    return g_activo;
}

HWND DocsEdit(int indice)
{
    if (indice < 0 || indice >= g_cantidad) return NULL;
    return g_documentos[indice].hEdit;
}

HWND DocsEditActivo(void)
{
    return DocsEdit(g_activo);
}

const WCHAR* DocsRuta(int indice)
{
    if (indice < 0 || indice >= g_cantidad) return L"";
    return g_documentos[indice].ruta;
}

CODIFICACION DocsCodificacion(int indice)
{
    if (indice < 0 || indice >= g_cantidad) return CODIF_UTF8;
    return g_documentos[indice].cod;
}

void DocsPonerRuta(int indice, const WCHAR* ruta)
{
    if (indice < 0 || indice >= g_cantidad) return;
    if (!ruta) ruta = L"";
    lstrcpyn(g_documentos[indice].ruta, ruta, MAX_PATH);
}

void DocsPonerCodificacion(int indice, CODIFICACION cod)
{
    if (indice < 0 || indice >= g_cantidad) return;
    g_documentos[indice].cod = cod;
}

/* ---------------------------------------------------------------
   DocsModificado: indica si el documento tiene cambios sin guardar.
   El control EDIT lleva la marca (EM_GETMODIFY), pero aquí se guarda
   una copia: el EDIT avisa cuando cambia (al escribir, al pegar, al
   deshacer...) y así la barra de pestañas puede saberlo sin tener que
   preguntar en cada repintado.
   --------------------------------------------------------------- */
BOOL DocsModificado(int indice)
{
    if (indice < 0 || indice >= g_cantidad) return FALSE;
    return g_documentos[indice].modificado;
}

void DocsPonerModificado(int indice, BOOL modificado)
{
    if (indice < 0 || indice >= g_cantidad) return;

    g_documentos[indice].modificado = modificado;

    /* La marca del propio EDIT se mantiene al día: es la que usa el
       control para saber si el contenido cambió */
    HWND hEdit = g_documentos[indice].hEdit;
    if (hEdit) SendMessage(hEdit, EM_SETMODIFY, (WPARAM)modificado, 0);
}

/* ---------------------------------------------------------------
   DocsIndiceDeRuta: busca el documento que corresponde a una ruta
   (para no abrir dos veces el mismo archivo)
   --------------------------------------------------------------- */
int DocsIndiceDeRuta(const WCHAR* ruta)
{
    if (!ruta || !ruta[0]) return -1;

    for (int i = 0; i < g_cantidad; i++) {
        if (_wcsicmp(g_documentos[i].ruta, ruta) == 0) return i;
    }
    return -1;
}

/* ---------------------------------------------------------------
   DocsAjusteLinea: indica si el ajuste de línea está activado
   (preferencia común a todos los documentos)
   --------------------------------------------------------------- */
BOOL DocsAjusteLinea(void)
{
    return g_ajusteLinea;
}

/* ---------------------------------------------------------------
   CrearEditor: crea el control EDIT de un documento. Nace oculto;
   DocsSeleccionar() decide cuál se ve.

   El ajuste de línea se elige aquí y no se puede cambiar después:
   el EDIT decide cómo maquetar el texto al crearse. Con ajuste, las
   líneas largas se parten al ancho del control; sin ajuste se añade
   ES_AUTOHSCROLL (y WS_HSCROLL) para poder desplazarse en horizontal.
   --------------------------------------------------------------- */
static HWND CrearEditor(HWND hwndPadre, BOOL ajustado)
{
    DWORD estilo = WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
                   ES_NOHIDESEL;

    if (!ajustado)
        estilo |= ES_AUTOHSCROLL | WS_HSCROLL;

    /* Sin WS_EX_CLIENTEDGE: con los temas de Windows 10/11 ese borde se
       pinta en el color de acento (una línea azul alrededor del texto).
       Sin él el editor queda plano y la pestaña activa continua con
       él, como en los editores al estilo Sublime Text. */
    HWND hEdit = CreateWindowEx(
        0, L"EDIT", L"", estilo,
        0, 0, 0, 0, hwndPadre, (HMENU)(INT_PTR)IDC_EDIT, g_hInst, NULL);

    if (!hEdit) return NULL;

    SendMessage(hEdit, EM_SETUNDOLIMIT, 100, 0);

    /* Cada EDIT lleva su propio subclase (atajos de teclado y sincronización
       con el panel de números y la barra de estado) */
    InstalarSubclaseEdit(hEdit);

    /* La tipografía vigente es una preferencia de la aplicación: también
       la usan los documentos que se abran más adelante */
    FuenteAplicarA(hEdit);

    return hEdit;
}

/* ---------------------------------------------------------------
   RecrearEditor: sustituye el EDIT de un documento por otro con el
   ajuste de línea indicado, conservando su texto, la selección, el
   desplazamiento vertical y la marca de modificado.

   El EDIT no permite cambiar el ajuste en caliente, de ahí la
   recreación. La pila de deshacer no se puede conservar: el control
   nuevo empieza con el historial vacío.
   --------------------------------------------------------------- */
static void RecrearEditor(HWND hwndPadre, int indice, BOOL ajustado)
{
    DOCUMENTO* doc = &g_documentos[indice];
    HWND hViejo = doc->hEdit;
    if (!hViejo) return;

    /* Copiar el texto antes de destruir el control */
    int largo = (int)SendMessage(hViejo, WM_GETTEXTLENGTH, 0, 0);
    WCHAR* texto = NULL;
    if (largo > 0) {
        texto = (WCHAR*)LocalAlloc(LPTR, (SIZE_T)(largo + 1) * sizeof(WCHAR));
        if (!texto) return;   /* sin memoria: se queda el EDIT anterior */
        SendMessage(hViejo, WM_GETTEXT, (WPARAM)(largo + 1), (LPARAM)texto);
    }

    DWORD inicio = 0, fin = 0;
    SendMessage(hViejo, EM_GETSEL, (WPARAM)&inicio, (LPARAM)&fin);

    int primeraVisible = (int)SendMessage(hViejo, EM_GETFIRSTVISIBLELINE, 0, 0);
    BOOL modificado = doc->modificado;

    /* Misma posición y tamaño que el control anterior */
    RECT rc;
    GetWindowRect(hViejo, &rc);
    MapWindowPoints(NULL, hwndPadre, (POINT*)&rc, 2);

    HWND hNuevo = CrearEditor(hwndPadre, ajustado);
    if (!hNuevo) {
        if (texto) LocalFree(texto);
        return;
    }

    if (texto) {
        SetWindowText(hNuevo, texto);
        LocalFree(texto);
    }

    MoveWindow(hNuevo, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
               FALSE);

    /* Restaurar selección, desplazamiento y marca de modificado. El orden
       importa: EM_SETSEL puede desplazar la vista para enseñar el cursor,
       así que el scroll se ajusta después, de forma absoluta. */
    SendMessage(hNuevo, EM_SETSEL, (WPARAM)inicio, (LPARAM)fin);

    int visibleAhora = (int)SendMessage(hNuevo, EM_GETFIRSTVISIBLELINE, 0, 0);
    if (primeraVisible != visibleAhora)
        SendMessage(hNuevo, EM_LINESCROLL, 0, (LPARAM)(primeraVisible - visibleAhora));

    SendMessage(hNuevo, EM_SETMODIFY, (WPARAM)modificado, 0);

    doc->hEdit = hNuevo;
    DestroyWindow(hViejo);
}

/* ---------------------------------------------------------------
   DocsCrear: añade un documento nuevo (con su EDIT) al final de la
   lista y devuelve su índice, o -1 si no se pudo crear.
   --------------------------------------------------------------- */
int DocsCrear(HWND hwndPadre, const WCHAR* ruta, CODIFICACION cod)
{
    if (g_cantidad >= MAX_DOCUMENTOS) return -1;

    HWND hEdit = CrearEditor(hwndPadre, g_ajusteLinea);
    if (!hEdit) return -1;

    DOCUMENTO* doc = &g_documentos[g_cantidad];
    doc->hEdit = hEdit;
    lstrcpyn(doc->ruta, ruta ? ruta : L"", MAX_PATH);
    doc->cod = cod;
    doc->modificado = FALSE;

    return g_cantidad++;
}

/* ---------------------------------------------------------------
   DocsEstablecerAjuste: activa o desactiva el ajuste de línea en
   todos los documentos. Es una preferencia de la ventana, así que
   se aplica a todos los EDIT abiertos y a los que se abran después.
   --------------------------------------------------------------- */
void DocsEstablecerAjuste(HWND hwndPadre, BOOL ajustado)
{
    if (ajustado == g_ajusteLinea) return;

    g_ajusteLinea = ajustado;

    for (int i = 0; i < g_cantidad; i++)
        RecrearEditor(hwndPadre, i, ajustado);

    /* Los EDIT nuevos aún no se ven: DocsSeleccionar muestra el activo,
       le devuelve el foco y refresca panel, barra de estado y pestañas */
    DocsSeleccionar(g_activo);
}

/* ---------------------------------------------------------------
   DocsSeleccionar: activa un documento. Muestra su EDIT, oculta los
   demás y actualiza el panel de números, la barra de estado, la barra
   de pestañas y el título de la ventana.
   --------------------------------------------------------------- */
void DocsSeleccionar(int indice)
{
    if (indice < 0 || indice >= g_cantidad) return;

    g_activo = indice;

    /* Sólo se ve el EDIT del documento activo */
    for (int i = 0; i < g_cantidad; i++) {
        HWND hEdit = g_documentos[i].hEdit;
        if (!hEdit) continue;
        BOOL visible = (i == indice);
        if (visible != (BOOL)IsWindowVisible(hEdit))
            ShowWindow(hEdit, visible ? SW_SHOW : SW_HIDE);
    }

    HWND hEdit = g_documentos[indice].hEdit;
    HWND hwnd = GetParent(hEdit);
    if (!hwnd) return;

    /* Al cambiar de documento hay que recargar su texto en lineas.c:
       Windows puede reutilizar el handle de un EDIT destruido y, en ese
       caso, la caché creería que sigue viendo el documento anterior. */
    LineasInvalidarTexto(hEdit);

    /* Si el EDIT que tenía el foco se acaba de ocultar, el foco pasa al
       documento activo: si no, los atajos de teclado dejarían de llegar
       al editor */
    HWND hFoco = GetFocus();
    if (!hFoco || hFoco == hwnd)
        SetFocus(hEdit);

    LineasReflow(hwnd);
    LineasActualizar(hEdit);
    ActualizarBarraEstado(hwnd);
    PestanasActualizar(hwnd);
    PonerTitulo(hwnd);
}

/* ---------------------------------------------------------------
   DocsCambiarActivo: pasa a la pestaña siguiente (delta = 1) o a la
   anterior (delta = -1), dando la vuelta por los extremos.
   --------------------------------------------------------------- */
void DocsCambiarActivo(int delta)
{
    if (g_cantidad <= 1) return;

    int nuevo = g_activo + delta;
    if (nuevo < 0) nuevo = g_cantidad - 1;
    if (nuevo >= g_cantidad) nuevo = 0;

    DocsSeleccionar(nuevo);
}

/* ---------------------------------------------------------------
   DocsColocarEdit: coloca todos los EDIT en la zona de edición.
   Los que no están activos quedan ocultos, pero se posicionan igual
   para que al activarlos no haya saltos.
   --------------------------------------------------------------- */
void DocsColocarEdit(HWND hwnd, int x, int y, int ancho, int alto)
{
    UNREFERENCED_PARAMETER(hwnd);

    for (int i = 0; i < g_cantidad; i++) {
        HWND hEdit = g_documentos[i].hEdit;
        if (hEdit) MoveWindow(hEdit, x, y, ancho, alto, TRUE);
    }
}

/* ---------------------------------------------------------------
   QuitarDocumento: quita un documento de la lista y destruye su
   EDIT. No pregunta nada: el aviso se ha hecho antes.
   --------------------------------------------------------------- */
static void QuitarDocumento(int indice)
{
    HWND hEdit = g_documentos[indice].hEdit;

    /* Quitar el documento de la lista */
    for (int i = indice; i < g_cantidad - 1; i++)
        g_documentos[i] = g_documentos[i + 1];
    g_cantidad--;

    /* El documento activo puede haberse desplazado o ser el que se cierra */
    int nuevoActivo = g_activo;
    if (g_activo == indice) {
        nuevoActivo = (indice < g_cantidad) ? indice : g_cantidad - 1;
    } else if (g_activo > indice) {
        nuevoActivo = g_activo - 1;
    }
    g_activo = (g_cantidad > 0) ? nuevoActivo : -1;

    if (hEdit) DestroyWindow(hEdit);
}

/* ---------------------------------------------------------------
   DocsCerrar: cierra un documento. Si tiene cambios sin guardar
   pregunta si se guardan (Guardar / No guardar / Cancelar). Nunca
   deja la aplicación sin pestañas: si era el último, se abre uno
   nuevo en blanco.
   --------------------------------------------------------------- */
BOOL DocsCerrar(HWND hwnd, int indice)
{
    if (indice < 0 || indice >= g_cantidad) return FALSE;

    /* Cambios sin guardar: preguntar antes de perderlos */
    if (DocsModificado(indice)) {
        if (!ConfirmarGuardado(hwnd, indice)) return FALSE;
    }

    QuitarDocumento(indice);

    /* Siempre queda al menos un documento abierto */
    if (g_cantidad == 0) {
        int nuevo = DocsCrear(hwnd, L"", CODIF_UTF8);
        DocsSeleccionar(nuevo);
    } else {
        DocsSeleccionar(g_activo);
    }

    return TRUE;
}

/* ---------------------------------------------------------------
   DocsCerrarTodos: cierra todos los documentos abiertos. Primero
   pregunta por los cambios sin guardar de cada uno, de atrás hacia
   delante: si el usuario cancela, no se cierra ninguno y todo se
   queda como estaba.

   Si dejarEnBlanco es cierto se abre después un documento nuevo y
   vacío (comando "Cerrar todo"); si no, la aplicación se queda sin
   documentos (se está cerrando).
   --------------------------------------------------------------- */
BOOL DocsCerrarTodos(HWND hwnd, BOOL dejarEnBlanco)
{
    for (int i = DocsCantidad() - 1; i >= 0; i--) {
        if (!DocsModificado(i)) continue;
        if (!ConfirmarGuardado(hwnd, i)) return FALSE;
    }

    while (g_cantidad > 0)
        QuitarDocumento(g_cantidad - 1);

    if (dejarEnBlanco) {
        int nuevo = DocsCrear(hwnd, L"", CODIF_UTF8);
        DocsSeleccionar(nuevo);
    }

    return TRUE;
}
