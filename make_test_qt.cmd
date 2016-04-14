@echo off
call "setup_Qt_x86.cmd"

cd build/sdl_win_qt_x86
call nmake

echo Run unit tests:
echo =================================================
%cd%\src\components\utils\test\utils_test.exe > nul /im /f 2>&1 &&  echo SUCCESS: utils_test passed ||  echo ERROR: utils_test failed 
%cd%\src\components\smart_objects\test\smart_object_test.exe > nul /im /f 2>&1 &&  echo SUCCESS: smart_object_test passed ||  echo ERROR: smart_object_test failed
%cd%\src\components\rpc_base\test\rpc_base_test.exe > nul /im /f 2>&1 &&  echo SUCCESS: rpc_base_test passed ||  echo ERROR: rpc_base_test failed
