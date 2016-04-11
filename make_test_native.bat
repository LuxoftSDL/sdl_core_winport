@echo off
call "setup_VS2013_x64.cmd"

cd build/sdl_win_x64

call "c:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe" smartDeviceLinkCore.sln /t:Rebuild /p:Configuration=Release

echo Run unit tests:
echo =================================================
%cd%\src\components\utils\test\Release\utils_test.exe > nul /im /f 2>&1 &&  echo SUCCESS: utils_test passed ||  echo ERROR: utils_test failed 
%cd%\src\components\smart_objects\test\Release\smart_object_test.exe > nul /im /f 2>&1 &&  echo SUCCESS: smart_object_test passed ||  echo ERROR: smart_object_test failed
%cd%\src\components\rpc_base\test\Release\rpc_base_test.exe > nul /im /f 2>&1 &&  echo SUCCESS: rpc_base_test passed ||  echo ERROR: rpc_base_test failed