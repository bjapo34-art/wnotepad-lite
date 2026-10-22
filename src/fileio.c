/* fileio.c - Manejo de archivos (Abrir, Guardar, Nuevo)
 *
 * Cada documento abierto tiene su propia ruta y su propia codificación
 * (las guarda documentos.c), así que las funciones de este módulo
 * trabajan siempre con el documento activo, salvo cuando se indica
 * expresamente otro índice.
 */

#define _WIN32_WINNT 0x0600
#define _WIN32_IE    0x0600
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include "app.h"
#include "resources.h"

/* Caché del tamaño en disco: se recalcula sólo cuando cambia la ruta,
   para no abrir el archivo en cada actualización de la barra de estado */
static WCHAR g_rutaTamano[MAX_PATH] = L"";
static DWORD g_tamanoCache = 0;

/* ---------------------------------------------------------------
   CodificacionActual: codificación con la que se guarda el documento
   actual (la detectada al abrir el archivo; UTF-8 para los nuevos).
   --------------------------------------------------------------- */
CODIFICACION CodificacionActual(void)
{
    return DocsCodificacion(DocsActivo());
}

/* ---------------------------------------------------------------
   PonerTitulo: Actualiza el título de la ventana con la ruta del
   documento activo
   --------------------------------------------------------------- */
void PonerTitulo(HWND hwnd)
{
    WCHAR titulo[512];
    const WCHAR* ruta = DocsRuta(DocsActivo());

    if (ruta && ruta[0])
        wsprintf(titulo, L"%s - Bloc de notas", ruta);
    else
        lstrcpy(titulo, L"Sin título - Bloc de notas");

    SetWindowText(hwnd, titulo);
}

/* ---------------------------------------------------------------
   ObtenerRutaActual: devuelve la ruta del documento abierto o una
   cadena vacía si todavía no se ha guardado ningún archivo
   --------------------------------------------------------------- */
const WCHAR* ObtenerRutaActual(void)
{
    return DocsRuta(DocsActivo());
}

/* ---------------------------------------------------------------
   InvalidarTamanoArchivo: fuerza a recalcular el tamaño del archivo
   la próxima vez que se consulte
   --------------------------------------------------------------- */
static void InvalidarTamanoArchivo(void)
{
    g_rutaTamano[0] = 0;
}

/* ---------------------------------------------------------------
   ObtenerTamanoArchivo: tamaño en bytes del archivo abierto
   (0 si no hay archivo). El valor se cachea por ruta.
   --------------------------------------------------------------- */
DWORD ObtenerTamanoArchivo(void)
{
    const WCHAR* ruta = ObtenerRutaActual();
    if (!ruta || !ruta[0]) return 0;

    /* ¿El tamaño cacheado corresponde a la ruta actual? */
    if (lstrcmpW(ruta, g_rutaTamano) != 0) {
        g_tamanoCache = 0;

        HANDLE h = CreateFile(ruta, GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            g_tamanoCache = GetFileSize(h, NULL);
            CloseHandle(h);
        }

        lstrcpyn(g_rutaTamano, ruta, MAX_PATH);
    }

    return g_tamanoCache;
}

/* ---------------------------------------------------------------
   NuevoDocumento: abre una pestaña nueva en blanco
   --------------------------------------------------------------- */
void NuevoDocumento(HWND hwnd)
{
    int indice = DocsCrear(hwnd, L"", CODIF_UTF8);
    if (indice < 0) {
        MessageBox(hwnd, L"No se pueden abrir más documentos",
                   L"Bloc de notas", MB_OK | MB_ICONWARNING);
        return;
    }

    /* Los documentos nuevos se guardan en UTF-8 */
    DocsPonerCodificacion(indice, CODIF_UTF8);
    DocsSeleccionar(indice);
}

/* ---------------------------------------------------------------
   AbrirDocumento: Muestra el diálogo de abrir y carga el archivo en
   una pestaña nueva. Si el archivo ya está abierto, se cambia a esa
   pestaña en lugar de abrirlo otra vez.
   --------------------------------------------------------------- */
void AbrirDocumento(HWND hwnd, HINSTANCE hInst)
{
    OPENFILENAME ofn = { sizeof(ofn) };
    WCHAR archivo[MAX_PATH] = L"";

    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Archivos de texto\0*.txt;*.log;*.ini;*.md\0Todos\0*.*\0";
    ofn.lpstrFile = archivo;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.hInstance = hInst;

    if (!GetOpenFileName(&ofn)) return; /* Cancelado por el usuario */

    /* Si el archivo ya está abierto, se va a su pestaña */
    int existente = DocsIndiceDeRuta(archivo);
    if (existente >= 0) {
        DocsSeleccionar(existente);
        return;
    }

    /* Abrir el archivo en modo lectura */
    HANDLE h = CreateFile(archivo, GENERIC_READ, FILE_SHARE_READ,
                          NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        MessageBox(hwnd, L"No se pudo abrir el archivo", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    /* Leer todo el contenido en memoria */
    DWORD tam = GetFileSize(h, NULL);
    BYTE* datos = (BYTE*)LocalAlloc(LPTR, (size_t)tam + 1);
    DWORD leidos = 0;

    if (!datos) {
        CloseHandle(h);
        MessageBox(hwnd, L"No hay memoria suficiente para abrir el archivo",
                   L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    if (tam > 0) {
        ReadFile(h, datos, tam, &leidos, NULL);
    }

    CloseHandle(h);

    /* Convertir el contenido a texto UTF-16 (detecta UTF-8, UTF-16 o ANSI) */
    WCHAR* texto = NULL;
    int largo = 0;
    CODIFICACION cod = CODIF_UTF8;

    if (!TextoDesdeBytes(datos, leidos, &texto, &largo, &cod)) {
        LocalFree(datos);
        MessageBox(hwnd, L"No se pudo interpretar el contenido del archivo",
                   L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    LocalFree(datos);

    /* Nueva pestaña con el archivo */
    int indice = DocsCrear(hwnd, archivo, cod);
    if (indice < 0) {
        LocalFree(texto);
        MessageBox(hwnd, L"No se pueden abrir más documentos",
                   L"Bloc de notas", MB_OK | MB_ICONWARNING);
        return;
    }

    HWND hEdit = DocsEdit(indice);

    /* Poner el texto en el EDIT del documento */
    SetWindowText(hEdit, texto);
    LocalFree(texto);

    SendMessage(hEdit, EM_EMPTYUNDOBUFFER, 0, 0);
    DocsPonerModificado(indice, FALSE);

    DocsSeleccionar(indice);
}

/* ---------------------------------------------------------------
   EscribirDocumento: guarda el texto de un documento en la ruta
   indicada, usando su codificación (UTF-8, UTF-16 o ANSI)
   --------------------------------------------------------------- */
static BOOL EscribirDocumento(int indice, const WCHAR* ruta)
{
    HWND hEdit = DocsEdit(indice);
    if (!hEdit) return FALSE;

    int len = GetWindowTextLength(hEdit);

    WCHAR* buf = NULL;
    if (len > 0) {
        buf = (WCHAR*)LocalAlloc(LPTR, ((size_t)len + 1) * sizeof(WCHAR));
        if (!buf) return FALSE;
        GetWindowText(hEdit, buf, len + 1);
    }

    BYTE* datos = NULL;
    DWORD tam = 0;

    if (!BytesDesdeTexto(buf ? buf : L"", len, DocsCodificacion(indice), &datos, &tam)) {
        if (buf) LocalFree(buf);
        return FALSE;
    }
    if (buf) LocalFree(buf);

    HANDLE h = CreateFile(ruta, GENERIC_WRITE, 0,
                          NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        LocalFree(datos);
        return FALSE;
    }

    DWORD escritos = 0;
    if (tam > 0) {
        WriteFile(h, datos, tam, &escritos, NULL);
    }

    CloseHandle(h);
    LocalFree(datos);
    return TRUE;
}

/* ---------------------------------------------------------------
   GuardarDocumento: guarda un documento. Si todavía no tiene ruta,
   pregunta por el nombre del archivo.
   --------------------------------------------------------------- */
BOOL GuardarDocumento(HWND hwnd, int indice)
{
    if (indice < 0 || indice >= DocsCantidad()) return FALSE;

    const WCHAR* ruta = DocsRuta(indice);
    WCHAR archivo[MAX_PATH];

    if (ruta && ruta[0]) {
        lstrcpyn(archivo, ruta, MAX_PATH);
    } else {
        /* Documento nuevo: hay que elegir dónde guardarlo */
        OPENFILENAME ofn = { sizeof(ofn) };
        archivo[0] = L'\0';

        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = L"Archivos de texto\0*.txt\0Todos\0*.*\0";
        ofn.lpstrFile = archivo;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = L"txt";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
        ofn.hInstance = g_hInst;

        if (!GetSaveFileName(&ofn)) return FALSE; /* Cancelado */
    }

    if (!EscribirDocumento(indice, archivo)) {
        MessageBox(hwnd, L"No se pudo guardar el archivo", L"Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    /* El archivo ya está en disco: el documento deja de estar modificado */
    DocsPonerRuta(indice, archivo);
    DocsPonerModificado(indice, FALSE);
    InvalidarTamanoArchivo();

    if (indice == DocsActivo()) {
        PonerTitulo(hwnd);
        ActualizarBarraEstado(hwnd);
    }
    PestanasActualizar(hwnd);

    return TRUE;
}

/* ---------------------------------------------------------------
   ConfirmarGuardado: pregunta qué hacer con los cambios sin guardar.
   Devuelve FALSE si el usuario cancela.
   --------------------------------------------------------------- */
BOOL ConfirmarGuardado(HWND hwnd, int indice)
{
    const WCHAR* ruta = DocsRuta(indice);
    const WCHAR* nombre = ruta;

    if (!ruta || !ruta[0]) {
        nombre = L"Sin título";
    } else {
        /* Sólo el nombre del archivo */
        for (const WCHAR* q = ruta; *q; q++) {
            if (*q == L'\\' || *q == L'/') nombre = q + 1;
        }
    }

    WCHAR msg[512];
    wsprintf(msg, L"¿Guardar los cambios en %s?", nombre);

    int r = MessageBox(hwnd, msg, L"Bloc de notas",
                        MB_YESNOCANCEL | MB_ICONQUESTION);

    if (r == IDCANCEL) return FALSE;
    if (r == IDYES) return GuardarDocumento(hwnd, indice);

    return TRUE;   /* No guardar */
}

/* ---------------------------------------------------------------
   GuardarComo: Muestra el diálogo de guardar y escribe el archivo
   --------------------------------------------------------------- */
void GuardarComo(HWND hwnd, HINSTANCE hInst)
{
    int indice = DocsActivo();
    if (indice < 0) return;

    OPENFILENAME ofn = { sizeof(ofn) };
    WCHAR archivo[MAX_PATH] = L"";

    /* Si ya hay una ruta, usarla como predeterminada */
    const WCHAR* ruta = DocsRuta(indice);
    if (ruta && ruta[0]) lstrcpyn(archivo, ruta, MAX_PATH);

    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Archivos de texto\0*.txt\0Todos\0*.*\0";
    ofn.lpstrFile = archivo;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    ofn.hInstance = hInst;

    if (!GetSaveFileName(&ofn)) return; /* Cancelado por el usuario */

    if (!EscribirDocumento(indice, archivo)) {
        MessageBox(hwnd, L"No se pudo guardar el archivo", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    /* Actualizar ruta y título */
    DocsPonerRuta(indice, archivo);
    DocsPonerModificado(indice, FALSE);
    InvalidarTamanoArchivo();

    PonerTitulo(hwnd);
    ActualizarBarraEstado(hwnd);
    PestanasActualizar(hwnd);
}

/* ---------------------------------------------------------------
   CerrarTodos: cierra todas las pestañas y deja una nueva en blanco
   --------------------------------------------------------------- */
static void CerrarTodos(HWND hwnd)
{
    DocsCerrarTodos(hwnd, TRUE);
}

/* ---------------------------------------------------------------
   ManejarComando: Procesa los comandos del menú y controles
   --------------------------------------------------------------- */
void ManejarComando(HWND hwnd, WPARAM wParam, HINSTANCE hInst)
{
    HWND hEdit = DocsEditActivo();

    switch (LOWORD(wParam)) {
    /* --- Archivo --- */
    case IDM_ARCHIVO_NUEVO:
        NuevoDocumento(hwnd);
        break;

    case IDM_ARCHIVO_ABRIR:
        AbrirDocumento(hwnd, hInst);
        break;

    case IDM_ARCHIVO_GUARDAR:
        GuardarDocumento(hwnd, DocsActivo());
        break;

    case IDM_ARCHIVO_GUARDAR_COMO:
        GuardarComo(hwnd, hInst);
        break;

    case IDM_ARCHIVO_CERRAR_PESTANA:
        DocsCerrar(hwnd, DocsActivo());
        break;

    case IDM_ARCHIVO_CERRAR_TODO:
        CerrarTodos(hwnd);
        break;

    case IDM_ARCHIVO_SALIR:
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        break;

    /* --- Edición --- */
    case IDM_EDIT_DESHACER:
        if (hEdit) SendMessage(hEdit, EM_UNDO, 0, 0);
        break;

    case IDM_EDIT_CORTAR:
        if (hEdit) SendMessage(hEdit, WM_CUT, 0, 0);
        break;

    case IDM_EDIT_COPIAR:
        if (hEdit) SendMessage(hEdit, WM_COPY, 0, 0);
        break;

    case IDM_EDIT_PEGAR:
        if (hEdit) SendMessage(hEdit, WM_PASTE, 0, 0);
        break;

    case IDM_EDIT_BORRAR:
        if (hEdit) SendMessage(hEdit, WM_CLEAR, 0, 0);
        break;

    case IDM_EDIT_SELECCIONAR:
        if (hEdit) SendMessage(hEdit, EM_SETSEL, 0, -1);
        break;

    case IDM_EDIT_FECHA_HORA: {
        SYSTEMTIME st;
        GetLocalTime(&st);
        WCHAR buf[64];
        GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, buf, 64);
        if (hEdit) SendMessage(hEdit, EM_REPLACESEL, TRUE, (LPARAM)buf);
        break;
    }

    case IDM_EDIT_IR_LINEA:
        MostrarIrALinea(hwnd);
        break;

    /* --- Formato --- */
    case IDM_FMT_AJUSTE: {
        /* Alternar el ajuste de línea (ajuste de palabras) del editor.
           El control EDIT ajusta las líneas cuando NO tiene ES_AUTOHSCROLL */
        if (hEdit) {
            LONG estilo = GetWindowLong(hEdit, GWL_STYLE);
            BOOL ajustado = (estilo & ES_AUTOHSCROLL) == 0;

            /* Es una preferencia de la ventana: se aplica a todos los
               documentos abiertos */
            DocsEstablecerAjuste(ajustado);

            CheckMenuItem(GetMenu(hwnd), IDM_FMT_AJUSTE,
                          MF_BYCOMMAND | (ajustado ? MF_UNCHECKED : MF_CHECKED));

            /* El ancho del EDIT cambia: hay que recolocar y repintar */
            RedrawWindow(hEdit, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
            LineasActualizar(hEdit);
        }
        break;
    }

    case IDM_FMT_FUENTE:
        MostrarFuente(hwnd);
        break;

    /* --- Ver --- */
    case IDM_VER_BARRA_ESTADO: {
        HWND hStatus = GetDlgItem(hwnd, IDC_STATUSBAR);
        if (hStatus) {
            BOOL visible = !IsWindowVisible(hStatus);
            ShowWindow(hStatus, visible ? SW_SHOW : SW_HIDE);
            CheckMenuItem(GetMenu(hwnd), IDM_VER_BARRA_ESTADO,
                          MF_BYCOMMAND | (visible ? MF_CHECKED : MF_UNCHECKED));
            ReflowBarraEstado(hwnd);
            LineasReflow(hwnd);
            ActualizarBarraEstado(hwnd);
        }
        break;
    }

    case IDM_VER_NUMEROS_LINEA: {
        HWND hPanel = GetDlgItem(hwnd, IDC_NUMEROS_LINEA);
        if (hPanel) {
            BOOL visible = !IsWindowVisible(hPanel);
            ShowWindow(hPanel, visible ? SW_SHOW : SW_HIDE);
            CheckMenuItem(GetMenu(hwnd), IDM_VER_NUMEROS_LINEA,
                          MF_BYCOMMAND | (visible ? MF_CHECKED : MF_UNCHECKED));
            LineasReflow(hwnd);
            if (visible) LineasActualizar(hEdit);
        }
        break;
    }

    /* --- Ayuda --- */
    case IDM_AYUDA_ACERCA:
        MessageBox(hwnd,
                   L"Bloc de notas Win32\nHecho con cl.exe y WinAPI pura",
                   L"Acerca de",
                   MB_OK | MB_ICONINFORMATION);
        break;
    }
}
