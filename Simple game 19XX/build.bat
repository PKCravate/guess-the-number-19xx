@echo off
rem Compile la version graphique (MSYS2 UCRT64 + SDL2 requis).
rem Installation SDL2 si besoin : pacman -S mingw-w64-ucrt-x86_64-SDL2
set PATH=C:\msys64\ucrt64\bin;%PATH%
for /f "delims=" %%i in ('pkg-config --cflags --static --libs sdl2') do set SDLFLAGS=%%i
gcc -std=c99 -Wall -O2 -o guess_gui.exe guess_gui.c %SDLFLAGS% -static
if errorlevel 1 (pause & exit /b 1)
echo OK : guess_gui.exe
