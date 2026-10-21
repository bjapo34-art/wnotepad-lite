#include <windows.h>
#include "resources.h"

HMENU ConstruirMenuPrincipal(void)
{
    HMENU hBarra = CreateMenu();
    HMENU hArchivo = CreatePopupMenu();
    AppendMenu(hArchivo, MF_STRING, IDM_ARCHIVO_NUEVO, L"&Nuevo\tCtrl+N");
    AppendMenu(hArchivo, MF_STRING, IDM_ARCHIVO_ABRIR, L"&Abrir...\tCtrl+O");
    AppendMenu(hArchivo, MF_SEPARATOR, 0, NULL);
    AppendMenu(hArchivo, MF_STRING, IDM_ARCHIVO_GUARDAR, L"&Guardar\tCtrl+S");
    AppendMenu(hArchivo, MF_STRING, IDM_ARCHIVO_GUARDAR_COMO, L"Guardar &como...\tCtrl+Mayús+S");
    AppendMenu(hArchivo, MF_SEPARATOR, 0, NULL);
    AppendMenu(hArchivo, MF_STRING, IDM_ARCHIVO_SALIR, L"&Salir");

    HMENU hEdicion = CreatePopupMenu();
    AppendMenu(hEdicion, MF_STRING, IDM_EDIT_DESHACER, L"&Deshacer\tCtrl+Z");
    AppendMenu(hEdicion, MF_SEPARATOR, 0, NULL);
    AppendMenu(hEdicion, MF_STRING, IDM_EDIT_CORTAR, L"&Cortar\tCtrl+X");
    AppendMenu(hEdicion, MF_STRING, IDM_EDIT_COPIAR, L"&Copiar\tCtrl+C");
    AppendMenu(hEdicion, MF_STRING, IDM_EDIT_PEGAR, L"P&egar\tCtrl+V");
    AppendMenu(hEdicion, MF_STRING, IDM_EDIT_BORRAR, L"E&liminar\tSupr");
    AppendMenu(hEdicion, MF_SEPARATOR, 0, NULL);
    AppendMenu(hEdicion, MF_STRING, IDM_EDIT_SELECCIONAR, L"&Seleccionar todo\tCtrl+E");
    AppendMenu(hEdicion, MF_STRING, IDM_EDIT_FECHA_HORA, L"/Hora y fecha\tF5");
    AppendMenu(hEdicion, MF_SEPARATOR, 0, NULL);
    AppendMenu(hEdicion, MF_STRING, IDM_EDIT_IR_LINEA, L"I&r a...\tCtrl+G");

    HMENU hFormato = CreatePopupMenu();
    AppendMenu(hFormato, MF_STRING | MF_CHECKED, IDM_FMT_AJUSTE, L"&Ajuste de línea");
    AppendMenu(hFormato, MF_STRING, IDM_FMT_FUENTE, L"F&uente...");

    HMENU hVer = CreatePopupMenu();
    AppendMenu(hVer, MF_STRING, IDM_VER_BARRA_ESTADO, L"&Barra de estado");

    HMENU hAyuda = CreatePopupMenu();
    AppendMenu(hAyuda, MF_STRING, IDM_AYUDA_ACERCA, L"&Acerca del Bloc de notas");

    AppendMenu(hBarra, MF_POPUP, (UINT_PTR)hArchivo, L"&Archivo");
    AppendMenu(hBarra, MF_POPUP, (UINT_PTR)hEdicion, L"&Edición");
    AppendMenu(hBarra, MF_POPUP, (UINT_PTR)hFormato, L"&Formato");
    AppendMenu(hBarra, MF_POPUP, (UINT_PTR)hVer, L"&Ver");
    AppendMenu(hBarra, MF_POPUP, (UINT_PTR)hAyuda, L"Ay&uda");
    return hBarra;
}