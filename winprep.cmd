@echo off

:runcmake
pushd "%BUILDDIR%" > NUL
if %errorlevel% == 1 goto builddir_error
shift


@echo on
pushd "%BUILDDIR%" > NUL
IF %_QT_PORT% == 1 (
   cmake -G %_SDL_GEN% -DQT_PORT=1 %SDL_ROOT% 
   @echo off
   popd
   goto end
) ELSE (
   cmake -G %_SDL_GEN% %SDL_ROOT% 
   @echo off
   popd
  goto end
)

:builddir_error
echo ERROR: Could not change to %BUILDDIR%
exit /b 1

:end
