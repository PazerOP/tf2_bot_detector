CALL vcvarsall.bat x86

signtool sign /tr http://timestamp.digicert.com /td sha256 /fd sha256 x64\tf2_bot_detector.exe x86\tf2_bot_detector.exe
