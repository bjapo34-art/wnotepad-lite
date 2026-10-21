#pragma once

#include <windows.h>
#include "resources.h"

/* main.c */
HWND CrearVentanaPrincipal(HINSTANCE hInst, int nCmdShow);   /* ← FALTABA ESTA */
void InstalarSubclaseEdit(HWND hEdit);

/* window.c */
LRESULT CALLBACK WndProcPrincipal(HWND, UINT, WPARAM, LPARAM);
void OnSize(HWND hwnd);
BOOL AlCerrarConfirmar(HWND hwnd);

/* menu.c */
HMENU ConstruirMenuPrincipal(void);

/* fileio.c */
void NuevoDocumento(HWND hwnd);
void AbrirDocumento(HWND hwnd, HINSTANCE hInst);
void GuardarComo(HWND hwnd, HINSTANCE hInst);
void ManejarComando(HWND hwnd, WPARAM wParam, HINSTANCE hInst);

/* dialogs.c */
void MostrarIrALinea(HWND hwnd);

/* undo_redo.c */
BOOL PuedeDeshacer(HWND hwnd);
void Deshacer(HWND hwnd);
void VaciarPilaUndo(HWND hwnd);