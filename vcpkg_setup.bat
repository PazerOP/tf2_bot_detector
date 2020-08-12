@ECHO OFF

PUSHD submodules\vcpkg
	SET VCPKG_ROOT=%CD%
	ECHO VCPKG_ROOT = %VCPKG_ROOT%
POPD

IF EXIST "%ProgramFiles(x86)%" (
	SET VCPKG_DEFAULT_TRIPLET=x64-windows
) else (
	SET VCPKG_DEFAULT_TRIPLET=x86-windows
)
ECHO VCPKG_DEFAULT_TRIPLET = %VCPKG_DEFAULT_TRIPLET%

SET VCPKG_EXE=%VCPKG_ROOT%\vcpkg.exe

SETLOCAL

IF NOT EXIST "%VCPKG_EXE%" (
	ECHO Building vcpkg...
		CALL %VCPKG_ROOT%\bootstrap-vcpkg.bat
		IF %ERRORLEVEL% NEQ 0 EXIT /B
)

ECHO Installing missing packages...
	%VCPKG_EXE% install @vcpkg_packages.txt
	IF %ERRORLEVEL% NEQ 0 EXIT /B

ECHO Upgrading existing packages...
	%VCPKG_EXE% upgrade --no-dry-run
	IF %ERRORLEVEL% NEQ 0 EXIT /B

ENDLOCAL
