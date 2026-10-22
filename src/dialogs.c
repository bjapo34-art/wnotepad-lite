#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <stdlib.h>
#include "app.h"
#include "resources.h"

// Añade en resources.h:  #define IDD_IR_LINEA 601

static INT_PTR CALLBACK IrLineaDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    UNREFERENCED_PARAMETER(lp);
    switch (msg) {
    case WM_INITDIALOG:
        SetFocus(GetDlgItem(hDlg, 1001));
        return FALSE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            WCHAR buf[16];
            GetDlgItemText(hDlg, 1001, buf, 16);
            int linea = _wtoi(buf);
            HWND hEdit = DocsEditActivo();

            int len = GetWindowTextLength(hEdit);
            WCHAR* txt = (WCHAR*)LocalAlloc(LPTR, (len + 1) * sizeof(WCHAR));
            if (txt) {
                GetWindowText(hEdit, txt, len + 1);
                int idx = 0, actual = 1;
                while (idx < len && actual < linea) {
                    if (txt[idx] == L'\n') actual++;
                    idx++;
                }
                SendMessage(hEdit, EM_SETSEL, idx, idx);
                SendMessage(hEdit, EM_SCROLLCARET, 0, 0);
                LocalFree(txt);
            }
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
    DialogBoxParam((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                   MAKEINTRESOURCE(IDD_IR_LINEA), hwnd, IrLineaDlgProc, 0);
}