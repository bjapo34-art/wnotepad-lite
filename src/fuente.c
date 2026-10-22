/* fuente.c - Diálogo "Formato > Fuente..." para elegir tipografía y tamaño
 *
 * El diálogo ofrece:
 *   - Lista de familias instaladas, cada una dibujada con su propia
 *     tipografía (igual que el selector de fuentes de Windows).
 *   - Combo de tamaños habituales, editable para poder escribir otro.
 *   - Combo de estilos: Regular, Negrita, Cursiva y Negrita cursiva.
 *   - Muestra en vivo de la fuente seleccionada.
 *
 * Al aceptar, la fuente se aplica al control EDIT con WM_SETFONT. El panel
 * de números de línea la hereda del EDIT y recalcula su ancho, por eso
 * después de aplicarla se llama a LineasReflow() y LineasActualizar().
 *
 * Al arrancar, el editor usa una tipografía monoespaciada (AplicarFuente-
 * PorDefecto) porque la fuente del sistema es proporcional y para un bloc
 * de notas de texto plano conviene una de anchura fija.
 */

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include <stdlib.h>
#include "app.h"
#include "resources.h"

/* Tamaños habituales, en puntos */
static const int TAMANOS_PUNTO[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72 };
#define NUM_TAMANOS ((int)(sizeof(TAMANOS_PUNTO) / sizeof(TAMANOS_PUNTO[0])))

/* Estilos de fuente disponibles */
#define ESTILO_REGULAR 0
#define ESTILO_NEGRITA 1
#define ESTILO_CURSIVA 2
#define ESTILO_AMBAS   3

static const WCHAR* ESTILOS[] = { L"Regular", L"Negrita", L"Cursiva", L"Negrita cursiva" };
#define NUM_ESTILOS ((int)(sizeof(ESTILOS) / sizeof(ESTILOS[0])))

/* Tamaño en puntos con el que se dibujan las familias en la lista */
#define PUNTOS_LISTA 11

/* Familias monoespaciadas candidatas para la fuente inicial del editor,
   por orden de preferencia */
static const WCHAR* FAMILIAS_MONO[] = {
    L"Consolas", L"Cascadia Mono", L"Courier New", L"Lucida Console"
};
#define NUM_FAMILIAS_MONO ((int)(sizeof(FAMILIAS_MONO) / sizeof(FAMILIAS_MONO[0])))

/* Tamaño en puntos de la fuente monoespaciada inicial */
#define PUNTOS_MONO_DEFECTO 11

/* Elección actual del diálogo */
static WCHAR g_familia[LF_FACESIZE] = L"Consolas";
static int   g_puntos = 11;
static int   g_estilo = ESTILO_REGULAR;

/* Fuente viva del editor: hay que conservarla mientras la usen el EDIT
   y el panel de números de línea */
static HFONT g_hFuenteEditor = NULL;

/* Fuente de la muestra del diálogo (se destruye al cerrar) */
static HFONT g_hFuenteMuestra = NULL;

/* ---------------------------------------------------------------
   DpiVertical: puntos por pulgada vertical de una ventana
   --------------------------------------------------------------- */
static int DpiVertical(HWND hwnd)
{
    HDC hdc = hwnd ? GetDC(hwnd) : NULL;
    if (!hdc) return 96;

    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hwnd, hdc);
    return dpi > 0 ? dpi : 96;
}

/* ---------------------------------------------------------------
   CrearFuente: crea una fuente a partir de familia, tamaño y estilo
   --------------------------------------------------------------- */
static HFONT CrearFuente(const WCHAR* familia, int puntos, int estilo, int dpi)
{
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));

    lstrcpyn(lf.lfFaceName, familia, LF_FACESIZE);
    lf.lfHeight = -MulDiv(puntos, dpi, 72);

    if (estilo == ESTILO_NEGRITA || estilo == ESTILO_AMBAS)
        lf.lfWeight = FW_BOLD;
    if (estilo == ESTILO_CURSIVA || estilo == ESTILO_AMBAS)
        lf.lfItalic = TRUE;

    lf.lfCharSet       = DEFAULT_CHARSET;
    lf.lfOutPrecision  = OUT_TT_PRECIS;
    lf.lfQuality       = CLEARTYPE_QUALITY;

    return CreateFontIndirectW(&lf);
}

/* ---------------------------------------------------------------
   AnchoCaracteres: mide el ancho total que ocupa un carácter
   repetido 16 veces. Se repite para no depender del redondeo al
   píxel: en una fuente de anchura fija el resultado es el mismo
   para cualquier carácter.
   --------------------------------------------------------------- */
static int AnchoCaracteres(HDC hdc, WCHAR caracter)
{
    WCHAR buf[16];
    for (int i = 0; i < 16; i++) buf[i] = caracter;

    SIZE sz;
    if (!GetTextExtentPoint32W(hdc, buf, 16, &sz)) return 0;
    return sz.cx;
}

/* ---------------------------------------------------------------
   EsFuenteMonoespaciada: comprueba que la fuente creada sea de
   anchura fija y que corresponda a la familia pedida. GDI sustituye
   las familias que no están instaladas por otra parecida, así que
   hay que mirar la fuente realmente seleccionada: si el ancho no es
   fijo o el nombre no es el pedido, esa familia no sirve.
   --------------------------------------------------------------- */
static BOOL EsFuenteMonoespaciada(HFONT hFuente, const WCHAR* familia)
{
    if (!hFuente) return FALSE;

    HDC hdc = CreateCompatibleDC(NULL);
    if (!hdc) return FALSE;

    HFONT hPrevia = (HFONT)SelectObject(hdc, hFuente);

    /* Nombre real de la tipografía que ha elegido GDI */
    WCHAR cara[LF_FACESIZE];
    cara[0] = L'\0';
    GetTextFaceW(hdc, LF_FACESIZE, cara);

    /* Anchura fija: todos los caracteres tienen el mismo avance.
       No se usa el bit FIXED_PITCH de TEXTMETRIC porque GDI no lo
       activa en las fuentes TrueType de ancho fijo, así que se
       comparan los avances reales de caracteres de formas muy
       distintas (una "i" estrecha y una "W" ancha). */
    static const WCHAR muestras[] = L"iWM0@";
    int ancho = AnchoCaracteres(hdc, muestras[0]);
    BOOL mono = ancho > 0;

    for (int i = 1; mono && muestras[i] != L'\0'; i++) {
        if (AnchoCaracteres(hdc, muestras[i]) != ancho)
            mono = FALSE;
    }

    if (hPrevia) SelectObject(hdc, hPrevia);
    DeleteDC(hdc);

    if (!mono) return FALSE;

    return _wcsicmp(cara, familia) == 0;
}

/* ---------------------------------------------------------------
   CrearFuenteMonoespaciada: crea una fuente de anchura fija.
   Se prueban varias familias por orden de preferencia y se
   comprueba que el resultado es realmente monoespaciado. Si
   ninguna sirve, se pide a Windows una fuente genérica de anchura
   fija.
   --------------------------------------------------------------- */
static HFONT CrearFuenteMonoespaciada(int puntos, int dpi)
{
    for (int i = 0; i < NUM_FAMILIAS_MONO; i++) {
        HFONT hFuente = CrearFuente(FAMILIAS_MONO[i], puntos, ESTILO_REGULAR, dpi);
        if (!hFuente) continue;

        if (EsFuenteMonoespaciada(hFuente, FAMILIAS_MONO[i])) return hFuente;

        DeleteObject(hFuente);
    }
    /* Último recurso: la fuente genérica monoespaciada de Windows */
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -MulDiv(puntos, dpi, 72);
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;

    return CreateFontIndirectW(&lf);
}

/* ---------------------------------------------------------------
   Lista de familias de fuentes
   --------------------------------------------------------------- */
typedef struct {
    WCHAR** familia;
    int     cantidad;
    int     capacidad;
} LISTA_FUENTES;

static void LiberarLista(LISTA_FUENTES* lista)
{
    for (int i = 0; i < lista->cantidad; i++)
        free(lista->familia[i]);
    free(lista->familia);
    lista->familia = NULL;
    lista->cantidad = 0;
    lista->capacidad = 0;
}

static void AnadirFamilia(LISTA_FUENTES* lista, const WCHAR* nombre)
{
    /* La enumeración repite la familia por cada juego de caracteres, así
       que hay que descartar los duplicados */
    for (int i = 0; i < lista->cantidad; i++) {
        if (_wcsicmp(lista->familia[i], nombre) == 0) return;
    }

    if (lista->cantidad == lista->capacidad) {
        int capacidad = lista->capacidad ? lista->capacidad * 2 : 64;
        WCHAR** nueva = (WCHAR**)realloc(lista->familia, (size_t)capacidad * sizeof(WCHAR*));
        if (!nueva) return;
        lista->familia = nueva;
        lista->capacidad = capacidad;
    }

    size_t largo = wcslen(nombre) + 1;
    WCHAR* copia = (WCHAR*)malloc(largo * sizeof(WCHAR));
    if (!copia) return;
    memcpy(copia, nombre, largo * sizeof(WCHAR));
    lista->familia[lista->cantidad++] = copia;
}

static int CALLBACK CallbackFamilias(const LOGFONTW* lf, const TEXTMETRICW* tm,
                                     DWORD tipo, LPARAM param)
{
    UNREFERENCED_PARAMETER(tm);
    UNREFERENCED_PARAMETER(tipo);

    /* Se descartan las variantes OEM (la 8514oem y compañía) y las
       fuentes privadas (las que empiezan por "@"), que no sirven para
       editar texto */
    if (lf && lf->lfFaceName[0] && lf->lfFaceName[0] != L'@' &&
        lf->lfCharSet != OEM_CHARSET)
        AnadirFamilia((LISTA_FUENTES*)param, lf->lfFaceName);
    return 1;
}

/* Comparación de nombres para ordenar la lista alfabéticamente */
static int CompararFamilias(const void* a, const void* b)
{
    const WCHAR* fa = *(const WCHAR* const*)a;
    const WCHAR* fb = *(const WCHAR* const*)b;
    return _wcsicmp(fa, fb);
}

static void EnumerarFamilias(LISTA_FUENTES* lista)
{
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;

    /* Hace falta un contexto gráfico real: con NULL la enumeración no
       devuelve ninguna familia */
    HDC hdc = GetDC(NULL);
    if (!hdc) return;

    EnumFontFamiliesExW(hdc, &lf, CallbackFamilias, (LPARAM)lista, 0);
    ReleaseDC(NULL, hdc);

    /* La lista se muestra ordenada alfabéticamente */
    if (lista->cantidad > 1)
        qsort(lista->familia, (size_t)lista->cantidad, sizeof(WCHAR*), CompararFamilias);
}

/* ---------------------------------------------------------------
   LeerFuenteEditor: recupera la tipografía que está usando el EDIT
   --------------------------------------------------------------- */
static void LeerFuenteEditor(HWND hwndPrincipal)
{
    HWND hEdit = GetDlgItem(hwndPrincipal, IDC_EDIT);
    HFONT hFuente = hEdit ? (HFONT)SendMessage(hEdit, WM_GETFONT, 0, 0) : NULL;

    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));

    /* Si el EDIT no tiene fuente propia usa la del sistema */
    if (!hFuente)
        hFuente = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    if (!hFuente || GetObjectW(hFuente, sizeof(lf), &lf) != sizeof(lf) || !lf.lfFaceName[0])
        return;   /* se conservan los valores por defecto */

    lstrcpyn(g_familia, lf.lfFaceName, LF_FACESIZE);

    int puntos = MulDiv(-lf.lfHeight, 72, DpiVertical(hwndPrincipal));
    if (puntos >= 1 && puntos <= 400)
        g_puntos = puntos;

    if (lf.lfWeight >= FW_BOLD && lf.lfItalic)      g_estilo = ESTILO_AMBAS;
    else if (lf.lfWeight >= FW_BOLD)                g_estilo = ESTILO_NEGRITA;
    else if (lf.lfItalic)                           g_estilo = ESTILO_CURSIVA;
    else                                            g_estilo = ESTILO_REGULAR;
}

/* ---------------------------------------------------------------
   LeerSeleccion: recoge la familia, el tamaño y el estilo elegidos
   --------------------------------------------------------------- */
static void LeerSeleccion(HWND hDlg)
{
    HWND hFamilia = GetDlgItem(hDlg, IDC_FUENTE_FAMILIA);
    HWND hEstilo  = GetDlgItem(hDlg, IDC_FUENTE_ESTILO);

    /* Familia */
    int sel = (int)SendMessage(hFamilia, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR) {
        WCHAR familia[LF_FACESIZE];
        familia[0] = L'\0';
        SendMessage(hFamilia, LB_GETTEXT, (WPARAM)sel, (LPARAM)familia);
        if (familia[0])
            lstrcpyn(g_familia, familia, LF_FACESIZE);
    }

    /* Tamaño: el combo es editable, así que se lee el texto tecleado */
    WCHAR buf[16];
    GetDlgItemText(hDlg, IDC_FUENTE_TAMANO, buf, 16);
    int puntos = _wtoi(buf);
    if (puntos >= 1 && puntos <= 400)
        g_puntos = puntos;

    /* Estilo */
    int estilo = (int)SendMessage(hEstilo, CB_GETCURSEL, 0, 0);
    if (estilo >= 0 && estilo < NUM_ESTILOS)
        g_estilo = estilo;
}

/* ---------------------------------------------------------------
   ActualizarMuestra: refleja en el texto de muestra la fuente elegida
   --------------------------------------------------------------- */
static void ActualizarMuestra(HWND hDlg)
{
    HWND hMuestra = GetDlgItem(hDlg, IDC_FUENTE_MUESTRA);
    if (!hMuestra) return;

    HWND hPrincipal = GetWindow(hDlg, GW_OWNER);

    if (g_hFuenteMuestra) {
        DeleteObject(g_hFuenteMuestra);
        g_hFuenteMuestra = NULL;
    }

    g_hFuenteMuestra = CrearFuente(g_familia, g_puntos, g_estilo, DpiVertical(hPrincipal));
    if (g_hFuenteMuestra)
        SendMessage(hMuestra, WM_SETFONT, (WPARAM)g_hFuenteMuestra, TRUE);
}

/* ---------------------------------------------------------------
   AplicarFuenteAlEditor: instala una fuente en el EDIT y la guarda
   como la viva, liberando la anterior cuando ya nadie la usa.
   --------------------------------------------------------------- */
static void AplicarFuenteAlEditor(HWND hwndPrincipal, HFONT hNueva)
{
    HWND hEdit = GetDlgItem(hwndPrincipal, IDC_EDIT);
    if (!hEdit || !hNueva) return;

    HFONT hVieja = g_hFuenteEditor;
    g_hFuenteEditor = hNueva;

    SendMessage(hEdit, WM_SETFONT, (WPARAM)hNueva, TRUE);

    /* El panel de números hereda la fuente del EDIT y recalcula su ancho */
    LineasReflow(hwndPrincipal);
    LineasActualizar(hEdit);
    ActualizarBarraEstado(hwndPrincipal);

    /* La fuente anterior ya no la usa nadie: ni el EDIT ni el panel */
    if (hVieja) DeleteObject(hVieja);
}

/* ---------------------------------------------------------------
   AplicarFuente: aplica la fuente elegida al editor de texto
   --------------------------------------------------------------- */
static void AplicarFuente(HWND hwndPrincipal)
{
    HFONT hNueva = CrearFuente(g_familia, g_puntos, g_estilo, DpiVertical(hwndPrincipal));
    if (!hNueva) return;

    AplicarFuenteAlEditor(hwndPrincipal, hNueva);
}

/* ---------------------------------------------------------------
   AplicarFuentePorDefecto: deja el editor con una tipografía
   monoespaciada desde el arranque. El editor de texto nace con la
   fuente del sistema, que es proporcional; para un bloc de notas
   pensado para texto plano lo habitual es una fuente de anchura
   fija, que además facilita contar columnas.
   --------------------------------------------------------------- */
void AplicarFuentePorDefecto(HWND hwndPrincipal)
{
    HFONT hFuente = CrearFuenteMonoespaciada(PUNTOS_MONO_DEFECTO, DpiVertical(hwndPrincipal));
    if (!hFuente) return;   /* se mantiene la fuente del sistema */

    AplicarFuenteAlEditor(hwndPrincipal, hFuente);
}

/* ---------------------------------------------------------------
   FuenteDlgProc: procedimiento del diálogo de fuente
   --------------------------------------------------------------- */
static INT_PTR CALLBACK FuenteDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {

    case WM_INITDIALOG: {
        HWND hPrincipal = GetWindow(hDlg, GW_OWNER);
        LeerFuenteEditor(hPrincipal);

        /* Lista de familias instaladas */
        HWND hFamilia = GetDlgItem(hDlg, IDC_FUENTE_FAMILIA);
        LISTA_FUENTES lista = { 0 };
        EnumerarFamilias(&lista);
        for (int i = 0; i < lista.cantidad; i++)
            SendMessage(hFamilia, LB_ADDSTRING, 0, (LPARAM)lista.familia[i]);
        LiberarLista(&lista);

        int sel = (int)SendMessage(hFamilia, LB_SELECTSTRING, (WPARAM)-1, (LPARAM)g_familia);
        if (sel == LB_ERR)
            sel = (int)SendMessage(hFamilia, LB_ADDSTRING, 0, (LPARAM)g_familia);
        if (sel != LB_ERR)
            SendMessage(hFamilia, LB_SETCURSEL, (WPARAM)sel, 0);

        /* Tamaños habituales */
        HWND hTamano = GetDlgItem(hDlg, IDC_FUENTE_TAMANO);
        for (int i = 0; i < NUM_TAMANOS; i++) {
            WCHAR buf[16];
            wsprintf(buf, L"%d", TAMANOS_PUNTO[i]);
            SendMessage(hTamano, CB_ADDSTRING, 0, (LPARAM)buf);
        }

        WCHAR buf[16];
        wsprintf(buf, L"%d", g_puntos);
        SetWindowText(hTamano, buf);

        /* Estilos */
        HWND hEstilo = GetDlgItem(hDlg, IDC_FUENTE_ESTILO);
        for (int i = 0; i < NUM_ESTILOS; i++)
            SendMessage(hEstilo, CB_ADDSTRING, 0, (LPARAM)ESTILOS[i]);
        SendMessage(hEstilo, CB_SETCURSEL, (WPARAM)g_estilo, 0);

        ActualizarMuestra(hDlg);
        SetFocus(hFamilia);
        return TRUE;
    }

    case WM_COMMAND: {
        int id  = LOWORD(wp);
        int cod = HIWORD(wp);

        if (id == IDC_FUENTE_FAMILIA && cod == LBN_SELCHANGE) {
            LeerSeleccion(hDlg);
            ActualizarMuestra(hDlg);
            return TRUE;
        }

        if (id == IDC_FUENTE_TAMANO &&
            (cod == CBN_SELCHANGE || cod == CBN_EDITCHANGE)) {
            LeerSeleccion(hDlg);
            ActualizarMuestra(hDlg);
            return TRUE;
        }

        if (id == IDC_FUENTE_ESTILO && cod == CBN_SELCHANGE) {
            LeerSeleccion(hDlg);
            ActualizarMuestra(hDlg);
            return TRUE;
        }

        if (id == IDOK) {
            LeerSeleccion(hDlg);
            HWND hPrincipal = GetWindow(hDlg, GW_OWNER);
            if (hPrincipal) AplicarFuente(hPrincipal);
            EndDialog(hDlg, IDOK);
            return TRUE;
        }

        if (id == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    }

    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT mis = (LPMEASUREITEMSTRUCT)lp;
        if (mis->CtlType != ODT_LISTBOX) return FALSE;

        /* MEASUREITEMSTRUCT no lleva el handle del control: se obtiene
           a partir del identificador */
        HWND hLista = GetDlgItem(hDlg, (int)mis->CtlID);
        WCHAR familia[LF_FACESIZE];
        familia[0] = L'\0';

        int total = (int)SendMessage(hLista, LB_GETCOUNT, 0, 0);
        if (mis->itemID >= 0 && mis->itemID < (UINT)total)
            SendMessage(hLista, LB_GETTEXT, mis->itemID, (LPARAM)familia);

        /* Cada familia se dibuja en su propia tipografía: la altura del
           elemento es la de esa tipografía */
        mis->itemHeight = 16;
        if (familia[0]) {
            HFONT hFuente = CrearFuente(familia, PUNTOS_LISTA, ESTILO_REGULAR, 96);
            HDC hdc = GetDC(hLista);
            if (hdc) {
                HFONT hPrevia = hFuente ? (HFONT)SelectObject(hdc, hFuente) : NULL;
                TEXTMETRICW tm;
                GetTextMetricsW(hdc, &tm);
                mis->itemHeight = tm.tmHeight + tm.tmExternalLeading + 4;
                if (hPrevia) SelectObject(hdc, hPrevia);
                ReleaseDC(hLista, hdc);
            }
            if (hFuente) DeleteObject(hFuente);
        }
        return TRUE;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lp;
        if (dis->CtlType != ODT_LISTBOX) return FALSE;
        if (dis->itemID == (UINT)-1) return TRUE;

        HWND hLista = dis->hwndItem;
        WCHAR familia[LF_FACESIZE];
        familia[0] = L'\0';

        int total = (int)SendMessage(hLista, LB_GETCOUNT, 0, 0);
        if (dis->itemID < (UINT)total)
            SendMessage(hLista, LB_GETTEXT, dis->itemID, (LPARAM)familia);

        BOOL seleccionado = (dis->itemState & ODS_SELECTED) != 0;
        COLORREF fondo = seleccionado ? GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_WINDOW);
        COLORREF tinta = seleccionado ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_WINDOWTEXT);

        HBRUSH hBrocha = CreateSolidBrush(fondo);
        FillRect(dis->hDC, &dis->rcItem, hBrocha);
        DeleteObject(hBrocha);

        HFONT hFuente = CrearFuente(familia, PUNTOS_LISTA, ESTILO_REGULAR, 96);
        HFONT hPrevia = hFuente ? (HFONT)SelectObject(dis->hDC, hFuente) : NULL;

        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, tinta);
        DrawTextW(dis->hDC, familia, -1, &dis->rcItem,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

        if (hPrevia) SelectObject(dis->hDC, hPrevia);
        if (hFuente) DeleteObject(hFuente);
        return TRUE;
    }
    }

    return FALSE;
}

/* ---------------------------------------------------------------
   MostrarFuente: abre el diálogo "Fuente..." y aplica el resultado
   --------------------------------------------------------------- */
void MostrarFuente(HWND hwndPrincipal)
{
    DialogBoxParam((HINSTANCE)GetWindowLongPtr(hwndPrincipal, GWLP_HINSTANCE),
                   MAKEINTRESOURCE(IDD_FUENTE), hwndPrincipal, FuenteDlgProc, 0);

    /* La muestra es temporal: se destruye al cerrar el diálogo */
    if (g_hFuenteMuestra) {
        DeleteObject(g_hFuenteMuestra);
        g_hFuenteMuestra = NULL;
    }
}
