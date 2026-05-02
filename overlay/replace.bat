@echo off
echo Attendo chiusura dell'overlay...
:wait
tasklist /FI "IMAGENAME eq overlay.exe" 2>NUL | find /I /N "overlay.exe" >NUL
if "%ERRORLEVEL%"=="0" (
    timeout /t 1 /nobreak >NUL
    goto wait
)
echo Sostituzione overlay.exe...
move /Y overlay_new.exe overlay.exe
echo Fatto! Avvio overlay...
start "" overlay.exe
