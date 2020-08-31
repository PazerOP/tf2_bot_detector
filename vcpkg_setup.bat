@ECHO OFF

PUSHD submodules\vcpkg
    SET VCPKG_ROOT=%CD%
    ECHO VCPKG_ROOT = %VCPKG_ROOT%
POPD

SET VCPKG_EXE=%VCPKG_ROOT%\vcpkg.exe

IF NOT EXIST "%VCPKG_EXE%" (
    ECHO Building vcpkg...
        CALL %VCPKG_ROOT%\bootstrap-vcpkg.bat
        IF %ERRORLEVEL% NEQ 0 EXIT /B
)
