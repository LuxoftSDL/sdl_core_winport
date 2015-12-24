@echo off

set _NPARAM=%~1		
echo "%_NPARAM%"
IF /I "%_NPARAM%"=="" goto default_build
  set BUILD_DIR=%1
  goto finish


:default_build
set BUILD_DIR="%SDL_ROOT%\build"
:finish
set SDL_BUILD_DIR="%BUILD_DIR%\%SDL_BUILD%"
if not exist %SDL_BUILD_DIR% mkdir %SDL_BUILD_DIR%
if not exist %SDL_BUILD_DIR% goto error
goto end

:error
@echo "Could not create build directory %SDL_BUILD_DIR%"
exit /B 2

:end
