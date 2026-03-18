@echo off
setlocal EnableDelayedExpansion

:: Shortcut: collect-dlls.sh fuer ARM64-Cross-Compile (MINGW64-Host, GUACD_TARGET_ARCH=arm64)

set "MSYS2_ROOT=C:\msys64"

set "WIN_DIR=%~dp0"
if "%WIN_DIR:~-1%"=="\" set "WIN_DIR=%WIN_DIR:~0,-1%"

set "TMP_FILE=%TEMP%\guacd_msys2path.tmp"
"%MSYS2_ROOT%\usr\bin\cygpath.exe" -u "%WIN_DIR%" > "%TMP_FILE%" 2>nul
set /p MSYS2_DIR=< "%TMP_FILE%"
del "%TMP_FILE%" 2>nul

"%MSYS2_ROOT%\usr\bin\env.exe" ^
    MSYSTEM=MINGW64 ^
    PATH=/mingw64/bin:/usr/bin ^
    GUACD_TARGET_ARCH=arm64 ^
    "%MSYS2_ROOT%\usr\bin\bash.exe" -l -c "cd '!MSYS2_DIR!' && bash collect-dlls.sh"

set "EXIT_CODE=!ERRORLEVEL!"
echo.
if !EXIT_CODE! equ 0 (echo  Fertig. Bundle: %WIN_DIR%\guacd-bundle-arm64\) else (echo  Fehler ^(Exit-Code: !EXIT_CODE!^).)
echo.
pause
exit /b !EXIT_CODE!
