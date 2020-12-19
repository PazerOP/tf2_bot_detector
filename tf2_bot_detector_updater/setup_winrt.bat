@echo off
SETLOCAL
SETLOCAL EnableDelayedExpansion

nuget install "Microsoft.Windows.CppWinRT" -OutputDirectory nuget || EXIT /b 1

CD nuget || EXIT /b 1

PUSHD .
	CD "Microsoft.Windows.CppWinRT*" || EXIT /b 1

	CD bin || EXIT /b 1

	ECHO Generating winrt headers...
	cppwinrt.exe -in local -out include -verbose -overwrite || EXIT /b 1

	CD include || EXIT /b 1

	SET WINRT_INCLUDE_DIR=%CD%
POPD

ECHO %WINRT_INCLUDE_DIR% > winrt_include_dir.txt

ENDLOCAL
