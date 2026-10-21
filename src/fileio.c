#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include "resources.h"

static WCHAR g_rutaActual[MAX_PATH] = L"";

static void PonerTitulo(HWND hwnd)
{
    WCHAR titulo[512];
    if (g_rutaActual[0])
        wsprintf(titulo, L"%s - Bloc de notas", g_rutaActual);
    else
        lstrcpy(titulo, L"Sin título - Bloc de notas");
    SetWindowText(hwnd, titulo);
}

void NuevoDocumento(HWND hwnd)
{
    SetDlgItemText(hwnd, IDC_EDIT, L"");
    g_rutaActual[0] = 0;
    PonerTitulo(hwnd);
    SendDlgItemMessage(hwnd, IDC_EDIT, EM_EMPTYUNDOBUFFER, 0, 0);
}

void AbrirDocumento(HWND hwnd, HINSTANCE hInst)
{
    OPENFILENAME ofn = { sizeof(ofn) };
    WCHAR archivo[MAX_PATH] = L"";
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Archivos de texto\0*.txt;*.log;*.ini;*.md\0Todos\0*.*\0";
    ofn.lpstrFile = archivo;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.hInstance = hInst;

    if (!GetOpenFileName(&ofn)) return;

    HANDLE h = CreateFile(archivo, GENERIC_READ, FILE_SHARE_READ, NULL,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { MessageBox(hwnd, L"No se pudo abrir", L"Error", MB_OK); return; }

    DWORD tam = GetFileSize(h, NULL);
    WCHAR* buf = (WCHAR*)LocalAlloc(LPTR, (tam / 2 + 2) * sizeof(WCHAR));
    DWORD leidos = 0;
    ReadFile(h, buf, tam, &leidos, NULL);
    CloseHandle(h);
    buf[leidos / sizeof(WCHAR)] = 0;

    SetDlgItemText(hwnd, IDC_EDIT, buf);
    LocalFree(buf);
    lstrcpy(g_rutaActual, archivo);
    PonerTitulo(hwnd);
    SendDlgItemMessage(hwnd, IDC_EDIT, EM_EMPTYUNDOBUFFER, 0, 0);
}

void GuardarComo(HWND hwnd, HINSTANCE hInst)
{
    SAVEFILENAME sfn = { sizeof(sfn) }; // ver nota abajo
    (void)sfn;
    // Implementación análoga con GetSaveFileName + WriteFile
    // (se completa igual que AbrirDocumento pero con CreateFile/GENERIC_WRITE)
}

void ManejarComando(HWND hwnd, WPARAM wParam, HINSTANCE hInst)
{
    HWND hEdit = GetDlgItem(hwnd, IDC_EDIT);
    switch (LOWORD(wParam)) {
    case IDM_ARCHIVO_NUEVO:        NuevoDocumento(hwnd); break;
    case IDM_ARCHIVO_ABRIR:        AbrirDocumento(hwnd, hInst); break;
    case IDM_ARCHIVO_GUARDAR:      /* TODO: si hay ruta, Guardar; si no, GuardarComo */ break;
    case IDM_ARCHIVO_GUARDAR_COMO: /* GuardarComo(hwnd, hInst); */ break;
    case IDM_ARCHIVO_SALIR:        PostMessage(hwnd, WM_CLOSE, 0, 0); break;
    case IDM_EDIT_DESHACER:        SendMessage(hEdit, EM_UNDO, 0, 0); break;
    case IDM_EDIT_CORTAR:          SendMessage(hEdit, WM_CUT, 0, 0); break;
    case IDM_EDIT_COPIAR:          SendMessage(hEdit, WM_COPY, 0, 0); break;
    case IDM_EDIT_PEGAR:           SendMessage(hEdit, WM_PASTE, 0, 0); break;
    case IDM_EDIT_BORRAR:          SendMessage(hEdit, WM_CLEAR, 0, 0); break;
    case IDM_EDIT_SELECCIONAR:     SendMessage(hEdit, EM_SETSEL, 0, -1); break;
    case IDM_EDIT_FECHA_HORA: {
        SYSTEMTIME st; GetLocalTime(&st);
        WCHAR buf[64];
        GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, buf, 64);
        SendMessage(hEdit, EM_REPLACESEL, TRUE, (LPARAM)buf);
        break;
    }
    case IDM_AYUDA_ACERCA:
        MessageBox(hwnd, L"Bloc de notas Win32\nHecho con cl.exe", L"Acerca de", MB_OK | MB_ICONINFORMATION);
        break;
    }
}