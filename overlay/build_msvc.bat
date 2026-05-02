@echo off
cl /EHsc /W3 /O2 /std:c++17 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS ^
    main.cpp ^
    /Fe:overlay.exe ^
    /link user32.lib gdi32.lib gdiplus.lib /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup
if errorlevel 1 goto failed
echo.
echo Build succeeded: overlay.exe
goto done

:failed
echo.
echo Build FAILED.

:done
