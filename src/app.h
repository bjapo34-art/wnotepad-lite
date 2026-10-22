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
const WCHAR* ObtenerRutaActual(void);
DWORD ObtenerTamanoArchivo(void);

/* dialogs.c */
void MostrarIrALinea(HWND hwnd);

/* fuente.c */
void MostrarFuente(HWND hwnd);
void AplicarFuentePorDefecto(HWND hwnd);

/* undo_redo.c */
BOOL PuedeDeshacer(HWND hwnd);
void Deshacer(HWND hwnd);
void VaciarPilaUndo(HWND hwnd);

/* linenumbers.c */
HWND CrearPanelNumeros(HWND hwndPadre, HINSTANCE hInst);
void LineasActualizar(HWND hEdit);
void LineasReflow(HWND hwndPrincipal);
int  LineasAnchoRequerido(HWND hwndPrincipal);

/* codificacion.c */
typedef enum {
    CODIF_ANSI = 0,   /* página de códigos del sistema (Windows-1252) */
    CODIF_UTF8,       /* UTF-8                                       */
    CODIF_UTF16LE,    /* UTF-16 little endian                         */
    CODIF_UTF16BE     /* UTF-16 big endian                            */
} CODIFICACION;

CODIFICACION DetectarCodificacion(const BYTE* datos, DWORD tam);
BOOL TextoDesdeBytes(BYTE* datos, DWORD tam, WCHAR** salida, int* largo, CODIFICACION* cod);
BOOL BytesDesdeTexto(const WCHAR* texto, int len, CODIFICACION cod, BYTE** salida, DWORD* tam);
const WCHAR* NombreCodificacion(CODIFICACION cod);

/* fileio.c */
CODIFICACION CodificacionActual(void);

/* statusbar.c */
HWND CrearBarraEstado(HWND hwndPadre, HINSTANCE hInst);
void ActualizarBarraEstado(HWND hwnd);
void ReflowBarraEstado(HWND hwnd);
int  AltoAreaEdicion(HWND hwnd);
