@echo off
setlocal EnableDelayedExpansion
rem ==========================================================================
rem  Compila el Motor de Rutas Inter-Sedes (C++17 + raylib) en Windows.
rem  Busca g++ y raylib en:
rem    1) el PATH y la variable RAYLIB_DIR (carpeta con include\raylib.h y lib\libraylib.a)
rem    2) %USERPROFILE%\herramientas_c  (w64devkit + raylib-*_win64_mingw-w64 descomprimidos)
rem    3) C:\raylib  (instalador oficial de raylib, trae su propio compilador)
rem ==========================================================================
cd /d "%~dp0"

where g++ >nul 2>nul
if errorlevel 1 (
    if exist "%USERPROFILE%\herramientas_c\w64devkit\bin\g++.exe" (
        set "PATH=%USERPROFILE%\herramientas_c\w64devkit\bin;!PATH!"
    ) else if exist "C:\raylib\w64devkit\bin\g++.exe" (
        set "PATH=C:\raylib\w64devkit\bin;!PATH!"
    ) else (
        echo [ERROR] No se encontro g++. Instale raylib para Windows desde https://raysan5.itch.io/raylib
        pause
        exit /b 1
    )
)

if not defined RAYLIB_DIR (
    for /d %%D in ("%USERPROFILE%\herramientas_c\raylib-*_win64_mingw-w64") do set "RAYLIB_DIR=%%~fD"
)
set "RAYLIB_FLAGS="
if defined RAYLIB_DIR set "RAYLIB_FLAGS=-I"%RAYLIB_DIR%\include" -L"%RAYLIB_DIR%\lib""
if not defined RAYLIB_DIR if exist "C:\raylib\raylib\src\raylib.h" set "RAYLIB_FLAGS=-IC:\raylib\raylib\src -LC:\raylib\raylib\src"

set "FUENTES=src\main.cpp src\Grafo.cpp src\MinHeap.cpp src\TablaHash.cpp src\Utilidades.cpp src\Carga.cpp src\Enrutador.cpp src\Contingencias.cpp src\Escenario.cpp src\Horario.cpp src\Pruebas.cpp src\Mapa.cpp"

echo Compilando...
g++ -std=c++17 -Wall -Wextra -O2 %FUENTES% %RAYLIB_FLAGS% -o enrutador.exe -lraylib -lopengl32 -lgdi32 -lwinmm
if errorlevel 1 (
    echo [ERROR] Fallo la compilacion.
    pause
    exit /b 1
)
echo.
echo [OK] enrutador.exe listo.   enrutador.exe  ^|  enrutador.exe --consola  ^|  enrutador.exe --pruebas
if /i not "%1"=="--sin-ejecutar" enrutador.exe
endlocal
