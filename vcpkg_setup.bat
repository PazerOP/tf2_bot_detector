@ECHO OFF

SET VCPKG_ROOT=submodules\vcpkg

IF EXIST "%ProgramFiles(x86)%" (
	SET VCPKG_DEFAULT_TRIPLET=x64-windows
) else (
	SET VCPKG_DEFAULT_TRIPLET=x86-windows
)

SETLOCAL

ECHO Initializing vcpkg...
	CALL %VCPKG_ROOT%\bootstrap-vcpkg.bat
	IF %ERRORLEVEL% NEQ 0 EXIT /B

ECHO Installing missing packages...
	%VCPKG_ROOT%\vcpkg.exe install @vcpkg_packages.txt
	IF %ERRORLEVEL% NEQ 0 EXIT /B

ECHO Upgrading existing packages...
	%VCPKG_ROOT%\vcpkg.exe upgrade
	IF %ERRORLEVEL% NEQ 0 EXIT /B

ENDLOCAL
PAUSE
