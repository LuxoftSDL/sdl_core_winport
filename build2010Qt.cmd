@echo off

setlocal EnableExtensions EnableDelayedExpansion


rem *********************** Set MSVC 32bit environment *************************


@if exist "%VS100COMNTOOLS%..\..\VC\bin\vcvars32.bat" (
    @goto call_vcvars32bat
) else (
    @goto set_vcvars32_manually
)


:call_vcvars32bat

@echo Calling vcvars32.bat...
@call "%VS100COMNTOOLS%..\..\VC\bin\vcvars32.bat"
@goto vcvars_end


:set_vcvars32_manually

@echo Setting environment for using Microsoft Visual Studio 2010 x86 tools manually.

@call :GetVSCommonToolsDir
@if "%VS100COMNTOOLS%"=="" goto error_no_VS100COMNTOOLSDIR

@call "%VS100COMNTOOLS%VCVarsQueryRegistry.bat" 32bit No64bit

@if "%VSINSTALLDIR%"=="" goto error_no_VSINSTALLDIR
@if "%FrameworkDir32%"=="" goto error_no_FrameworkDIR32
@if "%FrameworkVersion32%"=="" goto error_no_FrameworkVer32
@if "%Framework35Version%"=="" goto error_no_Framework35Version

@set FrameworkDir=%FrameworkDir32%
@set FrameworkVersion=%FrameworkVersion32%

@if not "%WindowsSdkDir%" == "" (
	@set "PATH=%WindowsSdkDir%bin\NETFX 4.0 Tools;%WindowsSdkDir%bin;%PATH%"
	@set "INCLUDE=%WindowsSdkDir%include;%INCLUDE%"
	@set "LIB=%WindowsSdkDir%lib;%LIB%"
)

@rem
@rem Root of Visual Studio IDE installed files.
@rem
@set DevEnvDir=%VSINSTALLDIR%Common7\IDE\

@rem PATH
@rem ----
@if exist "%VSINSTALLDIR%Team Tools\Performance Tools" (
	@set "PATH=%VSINSTALLDIR%Team Tools\Performance Tools;%PATH%"
)
@if exist "%ProgramFiles%\HTML Help Workshop" set PATH=%ProgramFiles%\HTML Help Workshop;%PATH%
@if exist "%ProgramFiles(x86)%\HTML Help Workshop" set PATH=%ProgramFiles(x86)%\HTML Help Workshop;%PATH%
@if exist "%VCINSTALLDIR%VCPackages" set PATH=%VCINSTALLDIR%VCPackages;%PATH%
@set PATH=%FrameworkDir%%Framework35Version%;%PATH%
@set PATH=%FrameworkDir%%FrameworkVersion%;%PATH%
@set PATH=%VSINSTALLDIR%Common7\Tools;%PATH%
@if exist "%VCINSTALLDIR%BIN" set PATH=%VCINSTALLDIR%BIN;%PATH%
@set PATH=%DevEnvDir%;%PATH%

@if exist "%VSINSTALLDIR%VSTSDB\Deploy" (
	@set "PATH=%VSINSTALLDIR%VSTSDB\Deploy;%PATH%"
)

@if not "%FSHARPINSTALLDIR%" == "" (
	@set "PATH=%FSHARPINSTALLDIR%;%PATH%"
)

@rem INCLUDE
@rem -------
@if exist "%VCINSTALLDIR%ATLMFC\INCLUDE" set INCLUDE=%VCINSTALLDIR%ATLMFC\INCLUDE;%INCLUDE%
@if exist "%VCINSTALLDIR%INCLUDE" set INCLUDE=%VCINSTALLDIR%INCLUDE;%INCLUDE%

@rem LIB
@rem ---
@if exist "%VCINSTALLDIR%ATLMFC\LIB" set LIB=%VCINSTALLDIR%ATLMFC\LIB;%LIB%
@if exist "%VCINSTALLDIR%LIB" set LIB=%VCINSTALLDIR%LIB;%LIB%

@rem LIBPATH
@rem -------
@if exist "%VCINSTALLDIR%ATLMFC\LIB" set LIBPATH=%VCINSTALLDIR%ATLMFC\LIB;%LIBPATH%
@if exist "%VCINSTALLDIR%LIB" set LIBPATH=%VCINSTALLDIR%LIB;%LIBPATH%
@set LIBPATH=%FrameworkDir%%Framework35Version%;%LIBPATH%
@set LIBPATH=%FrameworkDir%%FrameworkVersion%;%LIBPATH%

@goto vcvars_end

@REM -----------------------------------------------------------------------
:GetVSCommonToolsDir
@set VS100COMNTOOLS=
@call :GetVSCommonToolsDirHelper32 HKLM > nul 2>&1
@if errorlevel 1 call :GetVSCommonToolsDirHelper32 HKCU > nul 2>&1
@if errorlevel 1 call :GetVSCommonToolsDirHelper64  HKLM > nul 2>&1
@if errorlevel 1 call :GetVSCommonToolsDirHelper64  HKCU > nul 2>&1
@exit /B 0

:GetVSCommonToolsDirHelper32
@for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Microsoft\VisualStudio\SxS\VS7" /v "10.0"') DO (
	@if "%%i"=="10.0" (
		@SET "VS100COMNTOOLS=%%k"
	)
)
@if "%VS100COMNTOOLS%"=="" exit /B 1
@SET "VS100COMNTOOLS=%VS100COMNTOOLS%Common7\Tools\"
@exit /B 0

:GetVSCommonToolsDirHelper64
@for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VS7" /v "10.0"') DO (
	@if "%%i"=="10.0" (
		@SET "VS100COMNTOOLS=%%k"
	)
)
@if "%VS100COMNTOOLS%"=="" exit /B 1
@SET "VS100COMNTOOLS=%VS100COMNTOOLS%Common7\Tools\"
@exit /B 0

@REM -----------------------------------------------------------------------
:error_no_VS100COMNTOOLSDIR
@echo ERROR: Cannot determine the location of the VS Common Tools folder.
@goto vcvars_end

:error_no_VSINSTALLDIR
@echo ERROR: Cannot determine the location of the VS installation.
@goto vcvars_end

:error_no_FrameworkDIR32
@echo ERROR: Cannot determine the location of the .NET Framework 32bit installation.
@goto vcvars_end

:error_no_FrameworkVer32
@echo ERROR: Cannot determine the version of the .NET Framework 32bit installation.
@goto vcvars_end

:error_no_Framework35Version
@echo ERROR: Cannot determine the .NET Framework 3.5 version.
@goto vcvars_end

:vcvars_end


rem *********************** Set Qt environment variables ***********************


set QTDIR=C:\Qt\Qt5.5.1\5.5\msvc2010
set QMAKESPEC=win32-msvc2010
set PATH=%QTDIR%\bin;%PATH%


rem ************************* Set CMake generator ******************************


rem set _SDL_GEN="NMake Makefiles"
set _SDL_GEN="Visual Studio 10"

rem set PATH="%~dp0doxygen";%PATH%


rem ************************* Create build folder ******************************


rem call "%~d0%~p0\common.cmd" %*

set SDL_ROOT=%CD%

set _SHIFTTOK=0

set _NPARAM=%~1
IF /I "%_NPARAM%"=="" goto default_qt_build

set BUILDDIR=%1
shift
set _SHIFTTOK=1
goto create_builddir

:default_qt_build
set BUILDDIR="%CD..%%SDL_ROOT%-build_qt"

:create_builddir
IF NOT EXIST %BUILDDIR% mkdir ""%BUILDDIR%""
pushd ""%BUILDDIR%"" > NUL
if %errorlevel% == 1 goto error_creating_builddir
set BUILDDIR="%CD%"
popd > NUL

rem for /f "tokens=%_SHIFTTOK%*" %%a in ('echo.%*') do set CMAKE_PARAMS=%%b
goto end_builddir

:error_creating_builddir
echo "Could not create build directory %BUILDDIR%"
exit /B 2

:end_builddir
echo Using sources in: %SDL_ROOT%
rem echo Using projects in: %PROJDIR%
echo Generating build files in: %BUILDDIR%
echo Note that parameters for cmake should be enclosed in double quotes, e.g. "-DVERBOSE=1"


if %errorlevel% == 2 exit /B 1


rem *********************** Run cmake in the build folder **********************


rem call "%~d0%~p0\winprep.cmd"

:runcmake
pushd "%BUILDDIR%" > NUL
if %errorlevel% == 1 goto error_no_builddir
shift


@echo on
pushd "%BUILDDIR%" > NUL
cmake -G %_SDL_GEN% "-DQT_PORT=1" %SDL_ROOT% 
@echo off

popd
goto end_cmake

:error_no_builddir
echo ERROR: Could not change to %BUILDDIR%
exit /b 1

:end_cmake


rem ******************************** Build *************************************


rem @echo on
rem pushd "%BUILDDIR%" > NUL
rem rem nmake
rem msbuild smartDeviceLinkCore.sln
rem @echo off
rem popd

@echo.
@echo Now go to %BUILDDIR% and build the project with "msbuild smartDeviceLinkCore.sln"
