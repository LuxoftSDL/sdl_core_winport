@echo off 
call "%ProgramFiles(x86)%\Microsoft Visual Studio 12.0\VC\bin\x86_amd64\vcvarsx86_amd64.bat" x86_amd64
pushd %SQLITE_DIR% > NUL
lib /def:sqlite3.def /machine:x64
popd > NUL