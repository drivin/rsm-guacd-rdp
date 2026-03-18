@echo off
setlocal EnableDelayedExpansion

:: ============================================================================
:: run-build-arm64.bat  —  guacd ARM64-Cross-Compile per Doppelklick
::
:: Laeuft auf einem x64-Rechner (AMD64).
:: Kompiliert fuer Windows ARM64 mit Clang als Cross-Compiler.
:: Benoetigt: MSYS2 mit MINGW64- und CLANGARM64-Paketen.
:: ============================================================================

set "MSYS2_ROOT=C:\msys64"

:: --- MSYS2 pruefen ----------------------------------------------------------
if not exist "%MSYS2_ROOT%\msys2_shell.cmd" (
    echo.
    echo  FEHLER: MSYS2 nicht gefunden unter %MSYS2_ROOT%
    echo  Bitte MSYS2_ROOT am Anfang dieser Datei anpassen.
    echo.
    pause & exit /b 1
)

:: --- Verzeichnis dieser .bat-Datei ermitteln --------------------------------
set "WIN_DIR=%~dp0"
if "%WIN_DIR:~-1%"=="\" set "WIN_DIR=%WIN_DIR:~0,-1%"

:: --- Windows-Pfad in MSYS2-Pfad umwandeln -----------------------------------
set "TMP_FILE=%TEMP%\guacd_msys2path.tmp"

"%MSYS2_ROOT%\usr\bin\cygpath.exe" -u "%WIN_DIR%" > "%TMP_FILE%" 2>nul
if not exist "%TMP_FILE%" (
    echo  FEHLER: cygpath.exe konnte den Pfad nicht konvertieren.
    pause & exit /b 1
)
set /p MSYS2_DIR=< "%TMP_FILE%"
del "%TMP_FILE%" 2>nul

if "!MSYS2_DIR!"=="" (
    echo  FEHLER: Pfadkonvertierung ergab leere Zeichenkette.
    pause & exit /b 1
)

:: --- Shell-Befehl (GUACD_TARGET_ARCH=arm64 wird via env.exe propagiert) ---
set "SHELL_CMD=cd '!MSYS2_DIR!' && bash build.sh && bash collect-dlls.sh"

:: --- Ausgabe ----------------------------------------------------------------
echo.
echo  ================================================================
echo   guacd Windows ARM64 Build  (Cross-Compile von x64)
echo   Host       : AMD64 / x86-64
echo   Ziel       : Windows ARM64  (aarch64-w64-mingw32)
echo   Compiler   : Clang  (mingw-w64-x86_64-clang)
echo   Verzeichnis: %WIN_DIR%
echo   MSYS2-Pfad : !MSYS2_DIR!
echo  ================================================================
echo.

:: MSYSTEM=MINGW64 — x64-Host-Tools; GUACD_TARGET_ARCH=arm64 steuert build.sh
"%MSYS2_ROOT%\usr\bin\env.exe" ^
    MSYSTEM=MINGW64 ^
    PATH=/mingw64/bin:/usr/bin ^
    GUACD_TARGET_ARCH=arm64 ^
    "%MSYS2_ROOT%\usr\bin\bash.exe" -l -c "!SHELL_CMD!"

set "EXIT_CODE=!ERRORLEVEL!"

echo.
echo  ================================================================
if !EXIT_CODE! equ 0 (
    echo   BUILD ERFOLGREICH
    echo.
    echo   Portables Bundle:  %WIN_DIR%\guacd-bundle-arm64\
    echo.
    echo   Starten auf ARM64-Windows:
    echo     guacd-bundle-arm64\guacd.exe -f -b 127.0.0.1 -l 4822
) else (
    echo   BUILD FEHLGESCHLAGEN  ^(Exit-Code: !EXIT_CODE!^)
    echo.
    echo   Fehlerdetails in:
    echo     %WIN_DIR%\guacamole-server-1.6.0\build.log
    echo     %WIN_DIR%\configure.log
)
echo  ================================================================
echo.
pause
exit /b !EXIT_CODE!
