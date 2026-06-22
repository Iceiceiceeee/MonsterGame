@echo off
REM ============================================
REM  Monster Game - Windows 编译运行脚本
REM
REM  功能：
REM    自动设置 MSYS2 MinGW 环境变量，
REM    然后调用 mingw32-make 进行编译。
REM
REM  用法：
REM    make_run.bat          → 编译项目（等效于 mingw32-make）
REM    make_run.bat run      → 编译并运行
REM    make_run.bat clean    → 清理 build 目录
REM ============================================

REM 将 MSYS2 的 MinGW64 和核心工具路径添加到 PATH 环境变量中
set PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%

REM 根据传入的参数执行对应的 make 命令
REM 如果未传入参数，则默认执行 make（编译项目）
if "%1"=="" (
    mingw32-make
) else (
    mingw32-make %1
)
