#include <windows.h>
#include "resources.h"

// El control EDIT ya trae su propia pila de undo. Solo exponemos utilidades.
BOOL PuedeDeshacer(HWND hwnd)
{
    return (BOOL)SendDlgItemMessage(hwnd, IDC_EDIT, EM_CANUNDO, 0, 0);
}

void Deshacer(HWND hwnd)
{
    SendDlgItemMessage(hwnd, IDC_EDIT, EM_UNDO, 0, 0);
}

void VaciarPilaUndo(HWND hwnd)
{
    SendDlgItemMessage(hwnd, IDC_EDIT, EM_EMPTYUNDOBUFFER, 0, 0);
}