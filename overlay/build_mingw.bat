@echo off
pushd "%~dp0"
g++ -O2 -std=c++17 -municode -mwindows ^
    -DUNICODE -D_UNICODE ^
    main.cpp ^
    -o overlay.exe ^
    -luser32 -lgdi32 -lgdiplus
if errorlevel 1 goto failed
echo.
echo Build succeeded: overlay.exe
goto done

:failed
echo.
echo Build FAILED - make sure g++ is installed and in PATH.

:done
popd
