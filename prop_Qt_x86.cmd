@echo off 

if not defined QT_HOME_DIR (
@echo "ERROR: Set QT_HOME_DIR first. E.g. set QT_HOME_DIR=C:\Qt"
exit /b 1
)

call "%ProgramW6432%\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.cmd" /x86

rem set PATH="%~dp0doxygen";%PATH%

set _SDL_GEN="NMake Makefiles"
set build_suffics="qt5_build"
set _QT_PORT=1
set SDL_ROOT="%CD%"
set QTDIR=%QT_HOME_DIR%
set QMAKESPEC=win32-msvc2010

if %QTDIR% == ""  (
  call "%~d0%~p0\FindQt.cmd" %*
 if %errorlevel% == 2 exit /b 1
)

set PATH=%QTDIR%\bin;%PATH%

call "%~d0%~p0\common.cmd" %*
if %errorlevel% == 2 exit /b 1
call "%~d0%~p0\winprep.cmd"