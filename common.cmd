@echo off

set _SHIFTTOK=0

set _NPARAM=%~1
echo %_NPARAM%
IF /I "%_NPARAM%"=="" goto default_build
set BUILDDIR=%1
shift
set _SHIFTTOK=1

goto finish

:default_build
set BUILDDIR="%CD..%%SDL_ROOT%_%build_suffics%"
IF NOT EXIST %BUILDDIR% mkdir ""%BUILDDIR%""
pushd ""%BUILDDIR%"" > NUL
if %errorlevel% == 1 goto error
set BUILDDIR="%CD%"
popd > NUL

goto finished

:error
echo "Could not create build directory %BUILDDIR%"
exit /B 2

:finished
echo Using projects in: %PROJDIR%
echo Generating build files in: %BUILDDIR%

:end
