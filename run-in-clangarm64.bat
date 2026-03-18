@echo off
setlocal EnableDelayedExpansion

:: ============================================================================
:: run-in-clangarm64.bat  —  beliebiges Shell-Script in MSYS2 CLANGARM64 ausfuehren
::
:: Verwendung:
::   run-in-clangarm64.bat <script.sh> [arg1 arg2 ...]
:: ============================================================================

set "MSYS2_ROOT=C:\msys64"

if not exist "%MSYS2_ROOT%\msys2_shell.cmd" (
    echo.
    echo  FEHLER: MSYS2 nicht gefunden unter %MSYS2_ROOT%
    echo  Bitte MSYS2_ROOT am Anfang dieser Datei anpassen.
    echo.
    pause & exit /b 1
)

if "%~1"=="" (
    echo.
    echo  Verwendung: %~nx0 ^<script.sh^> [args...]
    echo.
    pause & exit /b 1
)

:: --- Pfadkonvertierung ueber Temp-Datei -------------------------------------
set "WIN_DIR=%~dp0"
if "%WIN_DIR:~-1%"=="\" set "WIN_DIR=%WIN_DIR:~0,-1%"

set "TMP_FILE=%TEMP%\guacd_msys2path.tmp"
"%MSYS2_ROOT%\usr\bin\cygpath.exe" -u "%WIN_DIR%" > "%TMP_FILE%" 2>nul
set /p MSYS2_DIR=< "%TMP_FILE%"
del "%TMP_FILE%" 2>nul

if "!MSYS2_DIR!"=="" (
    echo  FEHLER: Pfadkonvertierung ergab leere Zeichenkette.
    pause & exit /b 1
)

:: --- Argumente zusammensetzen -----------------------------------------------
set "SHELL_ARGS=%~1"
shift
:argloop
if not "%~1"=="" (
    set "SHELL_ARGS=!SHELL_ARGS! %~1"
    shift
    goto argloop
)

set "SHELL_CMD=cd '!MSYS2_DIR!' && bash !SHELL_ARGS!"

echo.
echo  MSYS2-Pfad : !MSYS2_DIR!
echo  Befehl     : !SHELL_CMD!
echo.

"%MSYS2_ROOT%\usr\bin\env.exe" ^
    MSYSTEM=CLANGARM64 ^
    PATH=/clangarm64/bin:/usr/bin ^
    "%MSYS2_ROOT%\usr\bin\bash.exe" -l -c "!SHELL_CMD!"

set "EXIT_CODE=!ERRORLEVEL!"
echo.
if !EXIT_CODE! equ 0 (echo  Fertig.) else (echo  Fehler ^(Exit-Code: !EXIT_CODE!^).)
echo.
pause
exit /b !EXIT_CODE!
