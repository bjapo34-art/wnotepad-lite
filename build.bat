@echo off
setlocal
if not exist build\obj mkdir build\obj

rc.exe /fo build\obj\notepad.res res\notepad.rc
if errorlevel 1 goto :fin

cl.exe /nologo /W4 /O2 /utf-8 /DUNICODE /D_UNICODE ^
  /Fobuild\obj\ src\main.c src\window.c src\menu.c src\fileio.c src\dialogs.c src\undo_redo.c src\linenumbers.c src\statusbar.c src\codificacion.c src\fuente.c src\documentos.c src\pestanas.c src\lineas.c ^
  %* ^
  build\obj\notepad.res ^
  /Fe:build\wnotepad.exe ^
  /link user32.lib gdi32.lib comdlg32.lib comctl32.lib

:fin
endlocal