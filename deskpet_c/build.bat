@echo off
REM ============================================================
REM  build.bat - Windows 原生编译脚本 (MinGW-w64)
REM  用法：双击运行，或命令行 build.bat
REM  需要：MinGW-w64 的 gcc / g++ / windres 在 PATH 中
REM ============================================================
setlocal

cd /d "%~dp0"

echo ============================================
echo   DeskPet 构建脚本
echo ============================================

where gcc >nul 2>nul
if errorlevel 1 (
    echo [错误] 找不到 gcc，请先安装 MinGW-w64 并加入 PATH
    echo        下载: https://www.mingw-w64.org/downloads/
    pause
    exit /b 1
)

if not exist build mkdir build

echo [1/4] 编译 C 源文件...
gcc -O2 -Wall -Wextra -std=c99 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 ^
    -municode -c src\main.c src\pet_anim.c src\pet_interact.c src\pet_window.c
if errorlevel 1 goto :fail

echo [2/4] 编译 C++ 渲染模块...
g++ -O2 -Wall -Wextra -std=c++17 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 ^
    -municode -fno-exceptions -fno-rtti -c src\pet_render.cpp
if errorlevel 1 goto :fail

echo [3/4] 编译资源...
windres -I src src\resource.rc -O coff -o build\resource.o
if errorlevel 1 goto :fail

echo [4/4] 链接...
move /y *.o build\ >nul 2>nul
g++ build\main.o build\pet_anim.o build\pet_interact.o build\pet_window.o ^
    build\pet_render.o build\resource.o -o build\DeskPet.exe ^
    -mwindows -municode -static -s ^
    -lgdiplus -lgdi32 -luser32 -lkernel32 -lcomctl32 -lole32 -luuid
if errorlevel 1 goto :fail

echo.
echo ============================================
echo   构建成功: build\DeskPet.exe
echo ============================================
echo.
echo 是否立即运行? (Y/N)
choice /c YN /n /t 10 /d N >nul
if not errorlevel 2 start "" build\DeskPet.exe
exit /b 0

:fail
echo.
echo [失败] 构建出错，请检查上方错误信息
pause
exit /b 1
