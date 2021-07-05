@ECHO OFF

REM Don't leak environment variables
SETLOCAL

IF EXIST tf2_bot_detector (
	ECHO Updating TF2 Bot Detector source...
		CD tf2_bot_detector
		git pull --recurse-submodules
		IF %ERRORLEVEL% NEQ 0 EXIT /B
) ELSE (
	ECHO Downloading TF2 Bot Detector source...
		git clone --depth=1 https://github.com/PazerOP/tf2_bot_detector.git tf2_bot_detector --recurse-submodules
		IF %ERRORLEVEL% NEQ 0 EXIT /B
		CD tf2_bot_detector
)

ECHO Compiling TF2 Bot Detector...
	RMDIR /S /Q build
	MKDIR build
	CD build
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=..\submodules\vcpkg\scripts\buildsystems\vcpkg.cmake ../
	IF %ERRORLEVEL% NEQ 0 EXIT /B
	cmake --build . --config Release
	IF %ERRORLEVEL% NEQ 0 EXIT /B

PUSHD ..\staging
	SET STAGING_DIR=%CD%
POPD

ECHO Copying default configuration...
	XCOPY /E /Y tf2_bot_detector\Release\* "%STAGING_DIR%"
	IF %ERRORLEVEL% NEQ 0 EXIT /B

IF NOT "%TF2BD_DL_AND_COMPILE_SKIP_OPEN%"=="1" (
	ECHO Opening compiled output...
		explorer "%STAGING_DIR%"
		IF %ERRORLEVEL% NEQ 0 EXIT /B
)

PAUSE
ENDLOCAL
