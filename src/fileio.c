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
   NuevoDocumento: Limpia el editor y resetea la ruta
   --------------------------------------------------------------- */
void NuevoDocumento(HWND hwnd)
{
    SetDlgItemText(hwnd, IDC_EDIT, L"");
    g_rutaActual[0] = 0;
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

    /* Obtener tamaño del archivo */
    DWORD tam = GetFileSize(h, NULL);
    
    /* Reservar memoria para el contenido (UTF-16) */
    WCHAR* buf = (WCHAR*)LocalAlloc(LPTR, (tam / 2 + 2) * sizeof(WCHAR));
    DWORD leidos = 0;
    
    if (tam > 0) {
        ReadFile(h, buf, tam, &leidos, NULL);
    }
    
    CloseHandle(h);
    
    /* Asegurar terminación nula */
    if (leidos > 0) {
        buf[leidos / sizeof(WCHAR)] = 0;
    } else {
        buf[0] = 0;
    }

    /* Poner texto en el EDIT y actualizar ruta */
    SetDlgItemText(hwnd, IDC_EDIT, buf);
    LocalFree(buf);
    
    lstrcpy(g_rutaActual, archivo);
    PonerTitulo(hwnd);
    SendDlgItemMessage(hwnd, IDC_EDIT, EM_EMPTYUNDOBUFFER, 0, 0);
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

    /* Abrir el archivo en modo escritura */
    HANDLE h = CreateFile(archivo, GENERIC_WRITE, 0, 
                          NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (h == INVALID_HANDLE_VALUE) {
        MessageBox(hwnd, L"No se pudo guardar el archivo", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    /* Obtener texto del EDIT */
    int len = GetWindowTextLength(GetDlgItem(hwnd, IDC_EDIT));
    
    if (len > 0) {
        WCHAR* buf = (WCHAR*)LocalAlloc(LPTR, (len + 1) * sizeof(WCHAR));
        GetWindowText(GetDlgItem(hwnd, IDC_EDIT), buf, len + 1);
        
        DWORD escritos = 0;
        WriteFile(h, buf, len * sizeof(WCHAR), &escritos, NULL);
        
        LocalFree(buf);
    }
    
    CloseHandle(h);

    /* Actualizar ruta y título */
    lstrcpy(g_rutaActual, archivo);
    PonerTitulo(hwnd);
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
            HANDLE h = CreateFile(g_rutaActual, GENERIC_WRITE, 0, 
                                  NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                int len = GetWindowTextLength(hEdit);
                if (len > 0) {
                    WCHAR* buf = (WCHAR*)LocalAlloc(LPTR, (len + 1) * sizeof(WCHAR));
                    GetWindowText(hEdit, buf, len + 1);
                    DWORD escritos = 0;
                    WriteFile(h, buf, len * sizeof(WCHAR), &escritos, NULL);
                    LocalFree(buf);
                }
                CloseHandle(h);
            }
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
    case IDM_FMT_AJUSTE:
        /* TODO: Toggle ES_AUTOHSCROLL en el EDIT */
        break;

    case IDM_FMT_FUENTE:
        /* TODO: Mostrar diálogo de fuente */
        break;

    /* --- Ayuda --- */
    case IDM_AYUDA_ACERCA:
        MessageBox(hwnd, 
                   L"Bloc de notas Win32\nHecho con cl.exe y WinAPI pura", 
                   L"Acerca de", 
                   MB_OK | MB_ICONINFORMATION);
        break;
    }
}