@echo off 
IF NOT EXIST "%BUILDDIR%\openssl" (
pushd %BUILDDIR% > NUL
@echo "Clone OpenSSSl"
git.exe clone https://github.com/openssl/openssl.git
pop > NUL
)
IF NOT EXIST "C:\Build-OpenSSL-VC-64" (
pushd %BUILDDIR%\openssl > NUL
call "%ProgramFiles(x86)%\Microsoft Visual Studio 12.0\VC\bin\x86_amd64\vcvarsx86_amd64.bat" x86_amd64
git.exe checkout origin/OpenSSL_1_0_2-stable
@echo "Build OpenSSSl for VC x64"
perl.exe Configure VC-WIN64A --prefix=C:\Build-OpenSSL-VC-64
call ms\do_win64a
nmake -f ms\nt.mak
nmake -f ms\nt.mak install
setx OPENSSL_CONF C:\Build-OpenSSL-VC-64\bin\openssl.cfg /M
setx OPENSSL_DIR C:\\Build-OpenSSL-VC-64 /M
pop > NUL
)
:endl