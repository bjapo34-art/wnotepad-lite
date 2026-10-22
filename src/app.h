#pragma once

#include <windows.h>
#include "resources.h"

/* codificacion.c */
typedef enum {
    CODIF_ANSI = 0,   /* página de códigos del sistema (Windows-1252) */
    CODIF_UTF8,       /* UTF-8                                       */
    CODIF_UTF16LE,    /* UTF-16 little endian                         */
    CODIF_UTF16BE     /* UTF-16 big endian                            */
} CODIFICACION;

/* main.c */
extern HINSTANCE g_hInst;
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
void PonerTitulo(HWND hwnd);
BOOL GuardarDocumento(HWND hwnd, int indice);
BOOL ConfirmarGuardado(HWND hwnd, int indice);

/* dialogs.c */
void MostrarIrALinea(HWND hwnd);

/* fuente.c */
void MostrarFuente(HWND hwnd);
void AplicarFuentePorDefecto(HWND hwnd);
void FuenteAplicarA(HWND hEdit);

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

/* documentos.c - modelo de documentos (uno por pestaña) */
#define MAX_DOCUMENTOS 24

int  DocsCantidad(void);
int  DocsActivo(void);
HWND DocsEdit(int indice);
HWND DocsEditActivo(void);
const WCHAR* DocsRuta(int indice);
CODIFICACION DocsCodificacion(int indice);
void DocsPonerRuta(int indice, const WCHAR* ruta);
void DocsPonerCodificacion(int indice, CODIFICACION cod);
BOOL DocsModificado(int indice);
void DocsPonerModificado(int indice, BOOL modificado);
int  DocsIndiceDeRuta(const WCHAR* ruta);
int  DocsCrear(HWND hwndPadre, const WCHAR* ruta, CODIFICACION cod);
void DocsSeleccionar(int indice);
void DocsCambiarActivo(int delta);
BOOL DocsCerrar(HWND hwnd, int indice);
BOOL DocsCerrarTodos(HWND hwnd, BOOL dejarEnBlanco);
void DocsColocarEdit(HWND hwnd, int x, int y, int ancho, int alto);
void DocsEstablecerAjuste(BOOL ajustado);

/* pestanas.c - barra de pestañas */
HWND CrearPestanas(HWND hwndPadre, HINSTANCE hInst);
void PestanasReflow(HWND hwndPrincipal);
int  PestanasAlto(HWND hwndPrincipal);
void PestanasActualizar(HWND hwndPrincipal);
