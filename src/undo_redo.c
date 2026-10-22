#include <windows.h>
#include "app.h"
#include "resources.h"

// El control EDIT ya trae su propia pila de undo. Solo exponemos utilidades.
BOOL PuedeDeshacer(HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);

    HWND hEdit = DocsEditActivo();
    if (!hEdit) return FALSE;
    return (BOOL)SendMessage(hEdit, EM_CANUNDO, 0, 0);
}

void Deshacer(HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);

    HWND hEdit = DocsEditActivo();
    if (hEdit) SendMessage(hEdit, EM_UNDO, 0, 0);
}

void VaciarPilaUndo(HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);

    HWND hEdit = DocsEditActivo();
    if (hEdit) SendMessage(hEdit, EM_EMPTYUNDOBUFFER, 0, 0);
}
