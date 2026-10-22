# Estado de la sesión — Bloc de notas Win32

> Archivo de traspaso. Sirve para retomar el trabajo en una sesión nueva
> sin tener que reconstruir todo el contexto desde cero.
> Última actualización: 21/10/2026 (sesión del sistema de pestañas).

---

## 1. Dónde está el proyecto

```
C:\Users\Beto\Code\winapi\notepad      (repo git, rama main, sin remoto)
```

Último commit: `5ff59ca Anade un sistema de pestañas al editor`
(anterior: `acdfe64`). El árbol quedó limpio tras ese commit.

## 2. Compilar

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && build.bat'
```

- `build.bat` compila con `cl.exe /nologo /W4 /O2 /utf-8 /DUNICODE /D_UNICODE`
  y `rc.exe`. Debe salir **sin avisos** (requisito del proyecto).
- `build.bat` reenvía `%*` a `cl.exe`, así que admite flags extra
  (por ejemplo `/DDEBUG_LOG`).
- Salida: `build\notepad.exe`.

## 3. Arquitectura actual

Módulos nuevos (los dos últimos):

| Archivo | Contenido |
|---|---|
| `src/documentos.c` | Modelo de documentos: lista de documentos, cuál está activo, ruta/codificación/**modificado** de cada uno. Un `EDIT` por documento (conserva pila de deshacer, selección y scroll). |
| `src/pestanas.c` | Barra de pestañas dibujada a mano: pestañas redondeadas (región GDI + `FrameRgn`), X de cierre, punto de "modificado", flechas de desplazamiento, rueda y clic. Clase `NotepadPestanas`, `IDC_PESTANAS` = 1003, alto 26. |

Decisiones importantes:

- **Un control `EDIT` por documento**, todos con el mismo ID (`IDC_EDIT`).
  El activo es el único visible; se localizan con `DocsEditActivo()`.
  `GetDlgItem(hApp, 1000)` ya **no** sirve para encontrar el editor visible.
- El flag `modificado` vive en el modelo (`DOCUMENTO.modificado`), no se lee
  de `EM_GETMODIFY` salvo para sincronizarlo (`main.c`).
- La barra de pestañas se dibuja con región GDI (`FillRgn` + `FrameRgn`),
  no con `ArcTo` (el SDK 10.0.26100 declara `ArcTo` con 9 parámetros).
- `res/app.manifest` declara la dependencia de **Common-Controls v6**, que
  hace falta para `SetWindowSubclass` → activa los temas visuales.
- El EDIT se crea **sin `WS_EX_CLIENTEDGE`**: con los temas de Win10/11 ese
  borde se pinta en el color de acento (línea azul #0078D7). Sin él el
  editor queda plano y la pestaña activa "fluye" hacia el texto.
- `DocsCerrarTodos(hwnd, dejarEnBlanco)`: pregunta primero por **todos** los
  documentos modificados (de atrás hacia delante) y, si se cancela, no se
  cierra ninguno. `dejarEnBlanco = TRUE` deja una pestaña nueva vacía
  ("Cerrar todo"), `FALSE` es el cierre de la aplicación.

## 4. Funcionalidad implementada y comprobada

| Área | Estado |
|---|---|
| Pestañas múltiples con texto propio | ✓ |
| Clic en pestaña / clic en la X / rueda | ✓ |
| Ctrl+Tab, Ctrl+Mayús+Tab, Ctrl+1..9 | ✓ |
| Ctrl+N / O / S / Ctrl+Mayús+S / G | ✓ |
| Aviso Guardar / No guardar / Cancelar al cerrar | ✓ |
| Cancelar **no** cierra nada | ✓ |
| Cierre de la app: un aviso por documento modificado | ✓ |
| "Cerrar todo": avisa por cada modificado y deja 1 pestaña en blanco | ✓ |
| Nunca queda la app sin pestañas | ✓ |
| Deshacer independiente por pestaña | ✓ |
| La fuente se aplica a **todas** las pestañas; las nuevas la heredan | ✓ |
| Barra de estado de 4 partes (incluye la codificación) | ✓ |
| Números de línea alineados con el texto | ✓ |
| Codificaciones (8 casos de prueba) | ✓ 8/8 |
| Desplazamiento de la barra con muchas pestañas | ✓ |

Atajos: `Ctrl+N/O/S/G`, `Ctrl+Mayús+S`, `Ctrl+W`, `Ctrl+Tab`, `Ctrl+Shift+Tab`,
`Ctrl+1..9`, `Esc` cierra la aplicación.

Menú: "Cerrar &pestaña" (ID 106) y "Cerrar &todo" (ID 107).

## 5. Verificación por píxeles (el modelo no puede leer imágenes)

Herramientas en `C:\Users\Beto\AppData\Local\Temp\opencode` (fuera del repo):

| Herramienta | Uso |
|---|---|
| `tabtool.exe` | Cliente de prueba de la app (ver tabla de comandos abajo). |
| `analizap.exe` | Analizador de píxeles: `analizap.exe <bmp> [x0 y0 x1 y1] [ink]`. Histograma + tramos de color por fila. El hex sale en **orden BGR**. Con `ink` mide la altura vertical de la tinta (sirve para medir el tamaño de letra). |
| `dumpedit.exe` | Vuelca el texto del EDIT **visible** vía MSAA. |
| `sbtext.exe` | Vuelca las 4 partes de la barra de estado vía MSAA. |
| `*.ps1` | Scripts de prueba: `codtest2.ps1` (codificaciones), `cierre.ps1`, `cerrartodo.ps1`, `muchas.ps1`, `fuente.ps1`, `final.ps1`, `final2.ps1`, `estado.ps1`, `borde.ps1`, `repro.ps1`… |

Comandos de `tabtool.exe`:

```
info                      título, geometría, controles hijos y estado de cada EDIT
cmd <id>                  WM_COMMAND con ese id (101=nuevo, 106=cerrar pestaña, 107=cerrar todo, 302=fuente…)
cmdclose                  WM_CLOSE a la ventana principal
clicktab <x> <y>          clic (PostMessage) en la barra de pestañas
mouseclick <x> <y>        clic real con el ratón (coordenadas FÍSICAS de pantalla)
type <texto>              pega texto en el EDIT visible (vía portapapeles)
gettext                   vuelca el texto de todos los EDIT (vía portapapeles)
focus                     control con el foco en el hilo de la app
dlginfo / dlgclick <id> [título] / dlgcombo <id> <idx> [título]
shot <bmp>                captura de la ventana
shottab <bmp>             captura solo de la barra de pestañas
shotedit <bmp>            captura solo del EDIT visible
shotdlg <bmp> / shotwin / shotall
```

## 6. Trucos y trampas del entorno (importante)

- **La app es DPI-unaware**: el sistema **virtualiza a la mitad** las
  coordenadas del ratón (físico → lógico), incluso en mensajes enviados a
  mano. Las coordenadas de `tabtool` son **físicas**.
- Para pulsar la pestaña N (0-based): x físico = `N*144 + 72`; su X está en
  x físico = `N*144 + 264` (rango `[N*144+250, N*144+278]`).
- **Entre procesos no funcionan los mensajes con punteros**: `WM_GETTEXT`,
  `WM_SETTEXT`, `CB_ADDSTRING`… Para leer texto: `EM_SETSEL(0,-1)` +
  `WM_COPY` + portapapeles. Para elegir en un combo: `CB_SETCURSEL` (no
  notifica, pero el diálogo lee el texto al aceptar).
- `GetWindowTextLength` entre procesos devuelve 0 para un EDIT: usar
  `SendMessage(WM_GETTEXTLENGTH)`.
- `SendMessage` a la app **se cuelga** si la acción abre un diálogo modal:
  usar `PostMessage`.
- La ventana "HotKey Listener" (#32770) del sistema puede confundirse con
  el MessageBox de la app: filtrar por título ("Bloc de notas", "Fuente").
- El analizador de píxeles imprime los colores en **BGR**: p. ej. el punto
  naranja RGB(206,106,32) sale como `#206ACE`.
- La app escribe **siempre** con BOM en UTF-8 y UTF-16LE: al guardar, los
  archivos de prueba "utf8-sinbom" / "utf16-sinbom" cambian de tamaño
  (78 → 81 bytes, 130 → 132). El test lo informa, no cuenta como fallo.

## 7. Ideas pendientes / posibles mejoras

1. **Reordenar pestañas arrastrando**: descartado por decisión del usuario.
2. Pestañas con nombre editable (renombrar un documento sin guardarlo).
3. Menú contextual con la lista de pestañas (al estilo de Sublime).
4. Botón "×" de la barra para cerrar todo de golpe.
5. Recordar el tamaño/posición de la ventana entre ejecuciones.
6. Guardar el estado de las pestañas (archivos abiertos) al salir.
7. `docs/` con más documentación: `info.txt` está desactualizado (no
   menciona `documentos.c`, `pestanas.c`, `codificacion.c`, `statusbar.c`,
   `linenumbers.c`).

## 8. Reglas del proyecto (para seguir respetándolas)

- WinAPI pura en C, sin frameworks.
- `/W4` **sin avisos**.
- Comentarios y nombres de identificadores **en español**.
- No hacer commit sin que lo pida el usuario (hasta ahora: `main`, sin remoto).
