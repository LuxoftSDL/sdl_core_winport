@echo off
setlocal EnableDelayedExpansion

set CUSTOM_BUILD_DIR=%~1
if "!CUSTOM_BUILD_DIR!"=="" (
  set BUILD_DIR="%SDL_ROOT%\build"
) else (
  set BUILD_DIR=!CUSTOM_BUILD_DIR!
)
set SDL_BUILD_DIR="!BUILD_DIR!\%SDL_BUILD%"
if not exist !SDL_BUILD_DIR! mkdir !SDL_BUILD_DIR!
if not exist !SDL_BUILD_DIR! goto error
goto end

:error
@echo "Could not create build directory !SDL_BUILD_DIR!"
exit /B 2

:end
