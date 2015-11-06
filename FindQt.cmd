@echo off
set _NPARAM=%~1
set findConf=%~2
IF /I "%_NPARAM%" == "-v" (
 set qt_version = "5.5.1"
  IF /I "%findConf%" == "" ( 
        @echo "Not correct library name"
        goto eof
  ) ELSE (
    call :findConfig "%findConf%"
  )
  goto eof
)
IF /I "%_NPARAM%"="-b"(
 call :rootdrive "%SystemDrive%"
 goto eof
)

FOR /f "tokens=*" %%G IN ('wmic logicaldisk get name ^| findstr /V "Name"') DO (call :alldrivesearch "%%G")
GOTO eof
	
:alldrivesearch
 set drive=%1
 set drive_no_space=%drive: =%
 pushd %drive_no_space% > NUL
 FOR /F "tokens=*" %%G IN ('dir /b /s /p %drive_no_space%\qmake.exe ^2^>NUL') DO (
	if NOT "%%G" == "File Not Found" (
	    for /F "tokens=4,6" %%A in ('%%G --version ^| findstr "Using"') do (
		  if "%%A" == '5.5.1' (
			@echo %%B
		 )
	   )
	  goto eof
	)
 )
 popd > NUL
 GOTO eof

:rootdrive
 set drive=%1
 set drive_no_space=%drive: =%
 pushd %drive_no_space% > NUL
 FOR /F "tokens=*" %%G IN ('dir /b /s /p %drive%\qmake.exe ^2^>NUL') DO (
	if NOT "%%G" == "File Not Found" (
	    for /F "tokens=4,6" %%A in ('%%G --version ^| findstr "Using"') do (
		  if "%%A" == '5.5.1' (
			@echo %%B
		 )
	   )
	  goto eof
	)
 )
 popd > NUL
 GOTO eof

:findConfig
 set drive="C:"
  pushd %drive% > NUL
 FOR /F "tokens=*" %%G IN ('dir /b /s /p %drive%\%1 ^2^>NUL') DO (
	if NOT "%%G" == "File Not Found" (
      @echo "%%G"
      popd > NUL
      GOTO eof
	)
 )
 popd > NUL
GOTO eof
 
:eof


