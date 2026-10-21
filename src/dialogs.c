#include <windows.h>
#include "resources.h"

// Diálogo "Ir a la línea..." creado en memoria (sin .rc)
static INT_PTR CALLBACK IrLineaDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG:
        SetFocus(GetDlgItem(hDlg, 1001));
        return FALSE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            WCHAR buf[16];
            int linea = 1;
            GetDlgItemText(hDlg, 1001, buf, 16);
            linea = _wtoi(buf);
            HWND hEdit = GetParent(GetParent(hDlg)); // según jerarquía real
            // Cálculo del índice de carácter para la línea indicada
            int len = GetWindowTextLength(hEdit);
            WCHAR* txt = (WCHAR*)LocalAlloc(LPTR, (len + 1) * sizeof(WCHAR));
            GetWindowText(hEdit, txt, len + 1);
            int idx = 0, actual = 1;
            while (idx < len && actual < linea) {
                if (txt[idx] == L'\n') actual++;
                idx++;
            }
            SendMessage(hEdit, EM_SETSEL, idx, idx);
            SendMessage(hEdit, EM_SCROLLCARET, 0, 0);
            LocalFree(txt);
            EndDialog(hDlg, IDOK);
        } else if (LOWORD(wp) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
        }
        return TRUE;
    }
    return FALSE;
}

void MostrarIrALinea(HWND hwnd)
{
    // Plantilla de diálogo en memoria
    WORD plantilla[] = {
        DLGWINDOWEXTRA, 0, 0, DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, 200, 80, L"Ir a la línea", 8, L"MS Shell Dlg",
    };
    // Nota: para un ejemplo rápido basta DialogBoxIndirect; en producción usa el .rc
    // Aquí usamos un truco simple con MessageBox como marcador:
    MessageBox(hwnd, L"Diálogo 'Ir a' pendiente de plantilla .rc", L"Info", MB_OK);
}