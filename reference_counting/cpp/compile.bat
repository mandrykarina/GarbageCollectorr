@echo off
REM Компиляция проекта Reference Counting GC Tester
REM С JSON парсером nlohmann/json

cd /d "%~dp0"

echo ===================================================
echo    Compiling Reference Counting GC Tester
echo    (с nlohmann/json)
echo ===================================================

REM Пути
set INCLUDE_DIR=.\include
set SRC_DIR=.\src
set BUILD_DIR=.\build

REM Создать build папку если нужно
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Флаги компиляции
set CFLAGS=-std=c++17 -Wall -Wextra -O2
set INCLUDES=-I"%INCLUDE_DIR%"

echo.
echo [1/1] Compiling sources...
g++ %CFLAGS% %INCLUDES% ^
    "%SRC_DIR%\rc_heap.cpp" ^
    "%SRC_DIR%\reference_counter.cpp" ^
    "%SRC_DIR%\event_logger.cpp" ^
    "%SRC_DIR%\scenario_loader.cpp" ^
    "%SRC_DIR%\main.cpp" ^
    -o "%BUILD_DIR%\rc_tester.exe"

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Compilation failed!
    echo.
    echo Make sure you have nlohmann/json.hpp in include/nlohmann/
    pause
    exit /b 1
)

echo.
echo ===================================================
echo    Build successful!
echo    Executable: %BUILD_DIR%\rc_tester.exe
echo ===================================================
echo.
echo To run tests:
echo   cd ..\python
echo   python run.py
echo.
pause
