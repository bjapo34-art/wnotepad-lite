# wnotepad lite v1.0 📝⚡

**Versión actual:** v1.0

**wnotepad lite** es un editor de texto minimalista y ultraligero (alrededor de 140 kB) desarrollado en **C puro utilizando WinAPI**, diseñado para ofrecer una experiencia completamente nativa, rápida y libre de dependencias pesadas, incorporando características avanzadas como sistema de pestañas y números de línea.

---

## ✨ Características Principales

* **Ultraligero y Eficiente:** Escrito desde cero sin frameworks externos, logrando un archivo ejecutable optimizado de unos 140 kB.
* **Sistema de Pestañas Nativo:** Barra de pestañas dibujada a mano mediante GDI con soporte para botón de cierre (`X`), indicador visual de cambios pendientes y flechas de navegación.
* **Números de Línea:** Panel lateral dedicado para la numeración de líneas sincronizado con el área de edición. La numeración es **lógica**: con el ajuste de línea activo, las filas en las que se parte una línea larga no repiten número ni lo incrementan.
* **Gestión de Codificaciones:** Detección y conversión automática de codificaciones de texto (ANSI, UTF-8, UTF-16).
* **Personalización de Fuente:** Diálogo personalizado para la selección de tipografías y aplicación de estilos.
* **Ajuste de Línea Conmutable:** Las líneas largas pueden ajustarse al ancho de la ventana o desbordarlo con desplazamiento horizontal, desde **Formato > Ajuste de línea**.
* **Barra de Estado Dinámica:** Muestra en tiempo real la posición (línea/columna), tamaño del archivo, codificación actual y nombre del documento.
* **Funciones Esenciales:** Deshacer por pestaña (aprovechando la pila nativa del EDIT), diálogo "Ir a la línea" y gestión limpia de archivos.

---

## 🗂️ Estructura del Proyecto

El código fuente se encuentra modularizado de forma estricta para garantizar un mantenimiento limpio y directo:

```text
wnotepad-lite/
│
├── src/                        # Código fuente en C
│   ├── main.c                  # Punto de entrada: WinMain, clase de ventana y subclase del EDIT
│   ├── window.c                # WndProcPrincipal, OnSize (recolocar controles) y cierre de la app
│   ├── documentos.c            # Modelo de documentos: un EDIT por pestaña, datos y estado de cada uno
│   ├── pestanas.c              # Barra de pestañas dibujada a mano (región GDI, X, punto de cambios, flechas)
│   ├── menu.c                  # Construcción del menú (Archivo, Edición, Formato, Ver, Ayuda)
│   ├── dialogs.c               # Diálogos: "Acerca de", "Ir a la línea"
│   ├── fuente.c                # Diálogo "Formato > Fuente..." propio y aplicación de la tipografía
│   ├── fileio.c                # Abrir/Guardar (GetOpenFileName/GetSaveFileName), lectura/escritura
│   ├── codificacion.c          # Detección y conversión de codificaciones (ANSI, UTF-8, UTF-16)
│   ├── linenumbers.c           # Panel de números de línea dibujado a mano
│   ├── lineas.c                # Líneas lógicas del documento: traduce entre filas visuales y saltos reales
│   ├── statusbar.c             # Barra de estado (línea/columna, tamaño, codificación, nombre del archivo)
│   ├── undo_redo.c             # Utilidades de deshacer sobre la pila propia del EDIT activo
│   ├── app.h                   # API compartida entre módulos
│   └── resources.h             # IDs simbólicos: IDM_ARCHIVO_ABRIR, IDC_EDIT, IDC_PESTANAS, etc.
│
├── res/                        # Recursos
│   ├── app.manifest            # Manifest: activa los temas visuales (ComCtl32 v6)
│   └── notepad.rc              # Script de recursos (.rc): manifest y diálogos
│
├── build/                      # Artefactos generados (no versionar)
│   ├── obj/                    # Archivos objeto intermedios [.obj, .res]
│   └── wnotepad.exe            # Ejecutable final
│
├── build.bat                   # Script que invoca a rc.exe y cl.exe con las flags correctas
├── README.md                   # Este archivo: descripción, estructura, compilación y uso
├── LICENSE                     # Licencia MIT
└── .gitignore                  # build/, *.obj, *.exe, *.res, *.ilk, *.pdb
```

---

## 🛠️ Requisitos

* **Windows 10 u 11** (la aplicación usa los temas visuales de Common-Controls v6).
* **Visual Studio** (o Build Tools) con la carga de trabajo de **desarrollo de escritorio en C++**.
* **Windows SDK** (para `rc.exe`, `windows.h` y las bibliotecas del sistema).

No se necesitan dependencias externas: solo el compilador y el SDK de Windows.

---

## 🚀 Compilación

Desde una consola con el entorno de Visual Studio cargado (por ejemplo, *x64 Native Tools Command Prompt for VS*):

```bat
build.bat
```

O sin abrir esa consola, cargando el entorno a mano y ajustando la ruta a tu instalación:

```bat
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && build.bat'
```

`build.bat` compila con:

```text
cl.exe /nologo /W4 /O2 /utf-8 /DUNICODE /D_UNICODE
rc.exe
```

* Se compila con **`/W4` y sin avisos** (requisito del proyecto).
* El script reenvía sus argumentos a `cl.exe`, así que admite flags extra (por ejemplo `build.bat /DDEBUG_LOG`).
* El resultado queda en **`build\wnotepad.exe`**.

---

## ▶️ Uso

Ejecuta `build\wnotepad.exe`. La ventana arranca con una pestaña vacía y puede abrir varias a la vez.

### Atajos de teclado

| Atajo | Acción |
|---|---|
| `Ctrl+N` | Documento nuevo (pestaña nueva) |
| `Ctrl+O` | Abrir archivo |
| `Ctrl+S` | Guardar |
| `Ctrl+Mayús+S` | Guardar como |
| `Ctrl+W` | Cerrar la pestaña activa |
| `Ctrl+G` | Ir a la línea |
| `Ctrl+Tab` / `Ctrl+Mayús+Tab` | Pestaña siguiente / anterior |
| `Ctrl+1` … `Ctrl+9` | Saltar a la pestaña N |
| `Esc` | Cerrar la aplicación |

También puedes hacer clic en una pestaña para activarla, en su `X` para cerrarla o usar la rueda sobre la barra para desplazarte cuando hay muchas abiertas. En el menú **Archivo** están **Cerrar pestaña** y **Cerrar todo**.

En **Formato > Ajuste de línea** se alterna el ajuste de palabras: activado (marcado), las líneas largas se parten al ancho de la ventana; desactivado, la línea se mantiene completa y aparece una barra de desplazamiento horizontal.

Al cerrar una pestaña (o la aplicación) con cambios sin guardar, la app pregunta **Guardar / No guardar / Cancelar**. Si se cancela, no se cierra nada.

---

## 📝 Notas de implementación

* Se usa **un control `EDIT` por documento**: cada pestaña conserva su propia pila de deshacer, selección y posición de desplazamiento. El documento activo es el único visible.
* La barra de pestañas es un control propio (`NotepadPestanas`) dibujado con GDI: pestañas redondeadas mediante región (`FillRgn` + `FrameRgn`), botón de cierre y punto de "modificado".
* El editor se crea **sin `WS_EX_CLIENTEDGE`** para que los temas de Windows 10/11 no pinten el borde con el color de acento; así la pestaña activa "fluye" hacia el texto.
* Los archivos se guardan **siempre con BOM** en UTF-8 y UTF-16LE.
* El flag de "modificado" vive en el modelo de documentos (`DOCUMENTO.modificado`) y se sincroniza desde `EM_GETMODIFY`.
* El ajuste de línea lo fija el `EDIT` al crearse (presencia de `ES_AUTOHSCROLL`), así que al cambiarlo se recrean los editores conservando texto, selección, desplazamiento y marca de modificado. La pila de deshacer del control se reinicia al alternar.
* El panel de números, la barra de estado e **Ir a la línea** trabajan con **líneas lógicas** (las que terminan en un salto de línea real), no con las filas visuales que produce el ajuste. `src/lineas.c` mantiene una caché del texto y la invalida cuando el `EDIT` avisa de un cambio de contenido.
* `res/app.manifest` declara la dependencia de **Common-Controls v6**, necesaria para `SetWindowSubclass` y para los temas visuales.
* La aplicación es **DPI-unaware**: Windows escala la ventana y virtualiza las coordenadas del ratón.

---

## 📄 Licencia

Este proyecto se distribuye bajo la licencia **MIT**. Consulta el archivo [`LICENSE`](LICENSE) para el texto completo.

Copyright © 2026 jrgclzd.
