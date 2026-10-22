/* fileio.c - Manejo de archivos (Abrir, Guardar, Nuevo) */

#define _WIN32_WINNT 0x0600
#define _WIN32_IE    0x0600
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include "app.h"
#include "resources.h"

/* Variable global estática para rastrear el archivo actual */
static WCHAR g_rutaActual[MAX_PATH] = L"";

/* Codificación del documento actual: se detecta al abrir el archivo y se
   respeta al guardarlo, para no alterar el formato original */
static CODIFICACION g_codificacion = CODIF_UTF8;

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
    return g_codificacion;
}

/* ---------------------------------------------------------------
   PonerTitulo: Actualiza el título de la ventana con la ruta actual
   --------------------------------------------------------------- */
static void PonerTitulo(HWND hwnd)
{
    WCHAR titulo[512];
    if (g_rutaActual[0])
        wsprintf(titulo, L"%s - Bloc de notas", g_rutaActual);
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
    return g_rutaActual;
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
   EstablecerRutaActual: asigna la ruta del documento abierto e
   invalida la caché del tamaño del archivo
   --------------------------------------------------------------- */
static void EstablecerRutaActual(const WCHAR* ruta)
{
    lstrcpyn(g_rutaActual, ruta, MAX_PATH);
    InvalidarTamanoArchivo();
}

/* ---------------------------------------------------------------
   ObtenerTamanoArchivo: tamaño en bytes del archivo abierto
   (0 si no hay archivo). El valor se cachea por ruta.
   --------------------------------------------------------------- */
DWORD ObtenerTamanoArchivo(void)
{
    if (!g_rutaActual[0]) return 0;

    /* ¿El tamaño cacheado corresponde a la ruta actual? */
    if (lstrcmpW(g_rutaActual, g_rutaTamano) != 0) {
        g_tamanoCache = 0;

        HANDLE h = CreateFile(g_rutaActual, GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            g_tamanoCache = GetFileSize(h, NULL);
            CloseHandle(h);
        }

        lstrcpyn(g_rutaTamano, g_rutaActual, MAX_PATH);
    }

    return g_tamanoCache;
}

/* ---------------------------------------------------------------
   NuevoDocumento: Limpia el editor y resetea la ruta
   --------------------------------------------------------------- */
void NuevoDocumento(HWND hwnd)
{
    SetDlgItemText(hwnd, IDC_EDIT, L"");
    EstablecerRutaActual(L"");
    g_codificacion = CODIF_UTF8;   /* los documentos nuevos se guardan en UTF-8 */
    PonerTitulo(hwnd);
    SendDlgItemMessage(hwnd, IDC_EDIT, EM_EMPTYUNDOBUFFER, 0, 0);
}

/* ---------------------------------------------------------------
   AbrirDocumento: Muestra el diálogo de abrir y carga el archivo
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

    /* Poner texto en el EDIT y actualizar ruta */
    SetDlgItemText(hwnd, IDC_EDIT, texto);
    LocalFree(texto);

    g_codificacion = cod;

    EstablecerRutaActual(archivo);
    PonerTitulo(hwnd);
    SendDlgItemMessage(hwnd, IDC_EDIT, EM_EMPTYUNDOBUFFER, 0, 0);
}

/* ---------------------------------------------------------------
   EscribirDocumento: guarda el texto del EDIT en la ruta indicada,
   usando la codificación actual del documento (UTF-8, UTF-16 o ANSI)
   --------------------------------------------------------------- */
static BOOL EscribirDocumento(HWND hwnd, const WCHAR* ruta)
{
    HWND hEdit = GetDlgItem(hwnd, IDC_EDIT);
    int len = GetWindowTextLength(hEdit);

    WCHAR* buf = NULL;
    if (len > 0) {
        buf = (WCHAR*)LocalAlloc(LPTR, ((size_t)len + 1) * sizeof(WCHAR));
        if (!buf) return FALSE;
        GetWindowText(hEdit, buf, len + 1);
    }

    BYTE* datos = NULL;
    DWORD tam = 0;

    if (!BytesDesdeTexto(buf ? buf : L"", len, g_codificacion, &datos, &tam)) {
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
   GuardarComo: Muestra el diálogo de guardar y escribe el archivo
   --------------------------------------------------------------- */
void GuardarComo(HWND hwnd, HINSTANCE hInst)
{
    OPENFILENAME ofn = { sizeof(ofn) };
    WCHAR archivo[MAX_PATH] = L"";

    /* Si ya hay una ruta, usarla como predeterminada */
    if (g_rutaActual[0]) {
        lstrcpy(archivo, g_rutaActual);
    }

    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Archivos de texto\0*.txt\0Todos\0*.*\0";
    ofn.lpstrFile = archivo;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    ofn.hInstance = hInst;

    if (!GetSaveFileName(&ofn)) return; /* Cancelado por el usuario */

    if (!EscribirDocumento(hwnd, archivo)) {
        MessageBox(hwnd, L"No se pudo guardar el archivo", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    /* Actualizar ruta y título */
    EstablecerRutaActual(archivo);
    PonerTitulo(hwnd);
    ActualizarBarraEstado(hwnd);
}

/* ---------------------------------------------------------------
   ManejarComando: Procesa los comandos del menú y controles
   --------------------------------------------------------------- */
void ManejarComando(HWND hwnd, WPARAM wParam, HINSTANCE hInst)
{
    HWND hEdit = GetDlgItem(hwnd, IDC_EDIT);

    switch (LOWORD(wParam)) {
    /* --- Archivo --- */
    case IDM_ARCHIVO_NUEVO:
        NuevoDocumento(hwnd);
        break;

    case IDM_ARCHIVO_ABRIR:
        AbrirDocumento(hwnd, hInst);
        break;

    case IDM_ARCHIVO_GUARDAR:
        if (g_rutaActual[0]) {
            /* Guardar sobre la ruta actual */
            if (!EscribirDocumento(hwnd, g_rutaActual)) {
                MessageBox(hwnd, L"No se pudo guardar el archivo",
                           L"Error", MB_OK | MB_ICONERROR);
                break;
            }
            /* El tamaño en disco cambió: invalidar la caché */
            InvalidarTamanoArchivo();
            ActualizarBarraEstado(hwnd);
        } else {
            GuardarComo(hwnd, hInst);
        }
        break;

    case IDM_ARCHIVO_GUARDAR_COMO:
        GuardarComo(hwnd, hInst);
        break;

    case IDM_ARCHIVO_SALIR:
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        break;

    /* --- Edición --- */
    case IDM_EDIT_DESHACER:
        SendMessage(hEdit, EM_UNDO, 0, 0);
        break;

    case IDM_EDIT_CORTAR:
        SendMessage(hEdit, WM_CUT, 0, 0);
        break;

    case IDM_EDIT_COPIAR:
        SendMessage(hEdit, WM_COPY, 0, 0);
        break;

    case IDM_EDIT_PEGAR:
        SendMessage(hEdit, WM_PASTE, 0, 0);
        break;

    case IDM_EDIT_BORRAR:
        SendMessage(hEdit, WM_CLEAR, 0, 0);
        break;

    case IDM_EDIT_SELECCIONAR:
        SendMessage(hEdit, EM_SETSEL, 0, -1);
        break;

    case IDM_EDIT_FECHA_HORA: {
        SYSTEMTIME st;
        GetLocalTime(&st);
        WCHAR buf[64];
        GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, buf, 64);
        SendMessage(hEdit, EM_REPLACESEL, TRUE, (LPARAM)buf);
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

            if (ajustado)
                SetWindowLong(hEdit, GWL_STYLE, estilo | ES_AUTOHSCROLL);
            else
                SetWindowLong(hEdit, GWL_STYLE, estilo & ~ES_AUTOHSCROLL);

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