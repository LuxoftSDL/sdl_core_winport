@echo off 
call "%ProgramFiles(x86)%\Microsoft Visual Studio 12.0\VC\bin\x86_amd64\vcvarsx86_amd64.bat" x86_amd64

rem set PATH="%~dp0doxygen";%PATH%

set _SDL_GEN="Visual Studio 12 Win64"
set build_suffics="build"
set SDL_ROOT=%CD%
SET _QT_PORT=0

call "%~d0%~p0\common.cmd" %*
if %errorlevel% == 2 exit /b 1
call "%~d0%~p0\winprep.cmd"