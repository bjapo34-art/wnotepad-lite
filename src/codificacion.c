/* codificacion.c - Detección y conversión de codificaciones de texto
 *
 * Los archivos de texto pueden estar guardados en distintas codificaciones
 * (UTF-8, UTF-16 little/big endian o ANSI). Este módulo se encarga de:
 *
 *   - Detectar la codificación de un archivo.
 *   - Convertir el contenido del archivo a texto UTF-16, que es el formato
 *     que entiende el control EDIT.
 *   - Convertir el texto del editor de vuelta a bytes, respetando la
 *     codificación original del documento.
 */

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include "app.h"

/* Marca de orden de bytes (BOM) del formato UTF-8 */
static const BYTE BOM_UTF8[3] = { 0xEF, 0xBB, 0xBF };

/* ---------------------------------------------------------------
   NombreCodificacion: nombre legible de una codificación, tal y como
   se muestra en la barra de estado (incluye si se escribe con BOM).
   --------------------------------------------------------------- */
const WCHAR* NombreCodificacion(CODIFICACION cod)
{
    switch (cod) {
        case CODIF_UTF16LE: return L"UTF-16 LE (con BOM)";
        case CODIF_UTF16BE: return L"UTF-16 BE (con BOM)";
        case CODIF_ANSI:   return L"ANSI (Windows-1252)";
        case CODIF_UTF8:
        default:           return L"UTF-8 (con BOM)";
    }
}

/* ---------------------------------------------------------------
   EsUTF8Valido: comprueba si un bloque de bytes es UTF-8 válido.
   Se usa para distinguir un archivo UTF-8 sin BOM de uno ANSI.
   --------------------------------------------------------------- */
static BOOL EsUTF8Valido(const BYTE* d, DWORD n)
{
    DWORD i = 0;

    while (i < n) {
        BYTE b = d[i];

        /* ASCII */
        if (b < 0x80) { i++; continue; }

        /* Dos bytes: 110xxxxx 10xxxxxx */
        if (b >= 0xC2 && b <= 0xDF) {
            if (i + 1 >= n) return FALSE;
            if ((d[i + 1] & 0xC0) != 0x80) return FALSE;
            i += 2;
            continue;
        }

        /* Tres bytes: 1110xxxx 10xxxxxx 10xxxxxx */
        if (b >= 0xE0 && b <= 0xEF) {
            if (i + 2 >= n) return FALSE;
            if ((d[i + 1] & 0xC0) != 0x80 || (d[i + 2] & 0xC0) != 0x80) return FALSE;
            /* Secuencias sobrepuestas o fuera de rango */
            if (b == 0xE0 && d[i + 1] < 0xA0) return FALSE;
            if (b == 0xED && d[i + 1] >= 0xA0) return FALSE;
            i += 3;
            continue;
        }

        /* Cuatro bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        if (b >= 0xF0 && b <= 0xF4) {
            if (i + 3 >= n) return FALSE;
            if ((d[i + 1] & 0xC0) != 0x80 || (d[i + 2] & 0xC0) != 0x80 ||
                (d[i + 3] & 0xC0) != 0x80) return FALSE;
            if (b == 0xF0 && d[i + 1] < 0x90) return FALSE;
            if (b == 0xF4 && d[i + 1] >= 0x90) return FALSE;
            i += 4;
            continue;
        }

        /* 0x80-0xC1 y 0xF5-0xFF nunca son válidos en UTF-8 */
        return FALSE;
    }

    return TRUE;
}

/* ---------------------------------------------------------------
   TieneBytesAlto: indica si el bloque contiene bytes por encima de
   0x7F (caracteres multibyte). Si no los tiene, el texto es ASCII
   puro y se puede tratar como ANSI sin perder información.
   --------------------------------------------------------------- */
static BOOL TieneBytesAlto(const BYTE* d, DWORD n)
{
    for (DWORD i = 0; i < n; i++) {
        if (d[i] >= 0x80) return TRUE;
    }
    return FALSE;
}

/* ---------------------------------------------------------------
   DetectarCodificacion: deduce la codificación de un bloque de bytes
   --------------------------------------------------------------- */
CODIFICACION DetectarCodificacion(const BYTE* d, DWORD n)
{
    /* Un archivo vacío no tiene codificación: se trata como ANSI para
       que al guardarlo siga teniendo cero bytes */
    if (!d || n == 0) return CODIF_ANSI;

    /* Marca de orden de bytes explícita */
    if (n >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF)
        return CODIF_UTF8;
    if (n >= 2 && d[0] == 0xFF && d[1] == 0xFE)
        return CODIF_UTF16LE;
    if (n >= 2 && d[0] == 0xFE && d[1] == 0xFF)
        return CODIF_UTF16BE;

    /* UTF-16 sin BOM: los bytes nulos se repiten en las posiciones impares
       (o pares, si es big endian) */
    if (n >= 4 && d[1] == 0x00 && d[3] == 0x00 && d[0] != 0x00)
        return CODIF_UTF16LE;
    if (n >= 4 && d[0] == 0x00 && d[2] == 0x00 && d[3] != 0x00)
        return CODIF_UTF16BE;

    /* UTF-8 sin BOM: válido y con caracteres multibyte */
    if (EsUTF8Valido(d, n) && TieneBytesAlto(d, n))
        return CODIF_UTF8;

    /* ASCII puro o texto con bytes no válidos en UTF-8: página de códigos
       del sistema (Windows-1252 en un equipo en español) */
    return CODIF_ANSI;
}

/* ---------------------------------------------------------------
   NormalizarSaltosLinea: convierte los saltos de línea "\n" sueltos
   (sin "\r" delante) en "\r\n", que es lo que espera el control EDIT.
   Se reserva un búfer nuevo porque el texto puede crecer.
   --------------------------------------------------------------- */
static WCHAR* NormalizarSaltosLinea(const WCHAR* src, int len, int* nuevoLen)
{
    int saltos = 0;
    for (int i = 0; i < len; i++) {
        if (src[i] == L'\n' && (i == 0 || src[i - 1] != L'\r')) saltos++;
    }

    WCHAR* dst = (WCHAR*)LocalAlloc(LPTR, ((size_t)len + saltos + 1) * sizeof(WCHAR));
    if (!dst) return NULL;

    int j = 0;
    for (int i = 0; i < len; i++) {
        if (src[i] == L'\n' && (i == 0 || src[i - 1] != L'\r')) {
            dst[j++] = L'\r';
        }
        dst[j++] = src[i];
    }
    dst[j] = 0;

    *nuevoLen = j;
    return dst;
}

/* ---------------------------------------------------------------
   TextoDesdeBytes: convierte el contenido de un archivo a texto
   UTF-16 listo para el control EDIT.
   El búfer de entrada no se modifica.
   Devuelve FALSE si no hay memoria suficiente.
   --------------------------------------------------------------- */
BOOL TextoDesdeBytes(BYTE* datos, DWORD tam, WCHAR** salida, int* largo, CODIFICACION* cod)
{
    if (!salida || !largo) return FALSE;
    *salida = NULL;
    *largo = 0;

    CODIFICACION c = DetectarCodificacion(datos, tam);
    if (cod) *cod = c;

    BYTE* ini = datos;
    DWORD n = tam;

    /* Saltar la marca de orden de bytes, si la hay */
    if (c == CODIF_UTF8 && n >= 3 && ini[0] == 0xEF && ini[1] == 0xBB && ini[2] == 0xBF) {
        ini += 3; n -= 3;
    } else if (c == CODIF_UTF16LE && n >= 2 && ini[0] == 0xFF && ini[1] == 0xFE) {
        ini += 2; n -= 2;
    } else if (c == CODIF_UTF16BE && n >= 2 && ini[0] == 0xFE && ini[1] == 0xFF) {
        ini += 2; n -= 2;
    }

    WCHAR* buf = NULL;
    int cnt = 0;

    if (c == CODIF_UTF16LE || c == CODIF_UTF16BE) {
        /* Los datos ya están en UTF-16: sólo hay que reinterpretarlos */
        cnt = (int)(n / 2);
        buf = (WCHAR*)LocalAlloc(LPTR, ((size_t)cnt + 1) * sizeof(WCHAR));
        if (!buf) return FALSE;

        for (int i = 0; i < cnt; i++) {
            BYTE b0 = ini[i * 2];
            BYTE b1 = ini[i * 2 + 1];
            buf[i] = (c == CODIF_UTF16LE) ? (WCHAR)(b0 | (b1 << 8))
                                          : (WCHAR)(b1 | (b0 << 8));
        }
    } else {
        UINT cp = (c == CODIF_UTF8) ? CP_UTF8 : CP_ACP;
        LPCSTR entrada = (LPCSTR)(void*)ini;

        cnt = (n > 0) ? MultiByteToWideChar(cp, 0, entrada, (int)n, NULL, 0) : 0;

        /* Si la conversión falla (secuencia UTF-8 truncada al final, por
           ejemplo) se reintenta con la página de códigos del sistema */
        if (cnt == 0 && n > 0 && c == CODIF_UTF8) {
            c = CODIF_ANSI;
            if (cod) *cod = c;
            cnt = MultiByteToWideChar(CP_ACP, 0, entrada, (int)n, NULL, 0);
        }

        buf = (WCHAR*)LocalAlloc(LPTR, ((size_t)cnt + 1) * sizeof(WCHAR));
        if (!buf) return FALSE;

        if (cnt > 0)
            MultiByteToWideChar(cp, 0, entrada, (int)n, buf, cnt);
    }

    buf[cnt] = 0;

    /* Normalizar los saltos de línea para el EDIT */
    int largoNorm = cnt;
    WCHAR* texto = NormalizarSaltosLinea(buf, cnt, &largoNorm);
    LocalFree(buf);

    if (!texto) return FALSE;

    *salida = texto;
    *largo = largoNorm;
    return TRUE;
}

/* ---------------------------------------------------------------
   BytesDesdeTexto: convierte el texto del editor a bytes en la
   codificación indicada, lista para escribir en disco.
   --------------------------------------------------------------- */
BOOL BytesDesdeTexto(const WCHAR* texto, int len, CODIFICACION cod, BYTE** salida, DWORD* tam)
{
    if (!salida || !tam) return FALSE;
    *salida = NULL;
    *tam = 0;

    if (len < 0) len = 0;

    if (cod == CODIF_UTF16LE || cod == CODIF_UTF16BE) {
        BYTE* buf = (BYTE*)LocalAlloc(LPTR, (size_t)len * 2 + 4);
        if (!buf) return FALSE;

        DWORD pos = 0;
        if (cod == CODIF_UTF16LE) {
            buf[pos++] = 0xFF; buf[pos++] = 0xFE;
        } else {
            buf[pos++] = 0xFE; buf[pos++] = 0xFF;
        }

        for (int i = 0; i < len; i++) {
            WCHAR c = texto[i];
            BYTE b0 = (BYTE)(c & 0xFF);
            BYTE b1 = (BYTE)((c >> 8) & 0xFF);
            if (cod == CODIF_UTF16LE) {
                buf[pos++] = b0; buf[pos++] = b1;
            } else {
                buf[pos++] = b1; buf[pos++] = b0;
            }
        }

        *salida = buf;
        *tam = pos;
        return TRUE;
    }

    UINT cp = (cod == CODIF_UTF8) ? CP_UTF8 : CP_ACP;
    int nb = (len > 0) ? WideCharToMultiByte(cp, 0, texto, len, NULL, 0, NULL, NULL) : 0;

    DWORD extra = (cod == CODIF_UTF8) ? 3 : 0;   /* BOM de UTF-8 */
    BYTE* buf = (BYTE*)LocalAlloc(LPTR, (size_t)nb + extra + 1);
    if (!buf) return FALSE;

    DWORD pos = 0;
    if (cod == CODIF_UTF8) {
        memcpy(buf, BOM_UTF8, 3);
        pos = 3;
    }

    if (nb > 0)
        WideCharToMultiByte(cp, 0, texto, len, (LPSTR)(void*)(buf + pos), nb, NULL, NULL);
    pos += (DWORD)nb;

    *salida = buf;
    *tam = pos;
    return TRUE;
}
