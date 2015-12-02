@echo off

IF "%1"=="" GOTO :usage

set build_type=%1

IF NOT EXIST %BUILDDIR%\openssl (
@echo "Clone OpenSSL"
git.exe clone https://github.com/openssl/openssl.git
)

IF %build_type%==x86 (
set CONFIG_OPT=VC-WIN32 no-asm
set ENV="%ProgramW6432%\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.cmd" /x86
) ELSE IF %build_type%==x64 (
set CONFIG_OPT=VC-WIN64A
set ENV="%ProgramFiles(x86)%\Microsoft Visual Studio 12.0\VC\bin\x86_amd64\vcvarsx86_amd64.bat" x86_amd64
) ELSE GOTO :usage

IF NOT EXIST %BUILDDIR%\openssl_build_%build_type% (
pushd %BUILDDIR%\openssl > NUL

call git.exe clean -fxd
call git.exe checkout origin/OpenSSL_1_0_2-stable

@echo "Build OpenSSl for VC %build_type%"

call %ENV%
perl.exe Configure %CONFIG_OPT% --prefix=%BUILDDIR%\openssl_build_%build_type%

IF %build_type%==x86 (
call ms\do_ms
) ELSE IF %build_type%==x64 (
call ms\do_win64a
) ELSE goto :usage

nmake -f ms\nt.mak
nmake -f ms\nt.mak install

popd > NUL
)

IF NOT $OPENSSL_DIR==%BUILDDIR%\openssl_build_%build_type% (
setx  OPENSSL_CONF %BUILDDIR%\openssl_build_%build_type%\bin\openssl.cfg
setx  OPENSSL_DIR %BUILDDIR%\openssl_build_%build_type% 
)

goto :endl

:usage
@echo "please specify either x86 or x64 build type"
:endl