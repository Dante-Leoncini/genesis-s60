@echo off
rem Compila segaw3d.exe para el N95 (ARMV5 urel, RVCT 2.2, el SDK de C:\epoc32: el mismo que usa Carbide) sin abrir
rem Carbide. Desde PowerShell o cmd (no desde git-bash: MSYS rompe los "cmd /c" de las herramientas de Symbian).
rem     compilar.bat          compila lo que cambio
rem     compilar.bat limpio   borra lo compilado antes
rem Despues: python probar.py (instala por el cable, prueba y baja el log).
setlocal
set PATH=%RVCT22BIN%;C:\epoc32\tools;C:\epoc32\gcc\bin;%PATH%
set EPOCROOT=\
cd /d "%~dp0group"
call bldmake bldfiles || goto error
if /i "%1"=="limpio" call perl -S ABLD.PL "%~p0group\\" reallyclean armv5 urel
rem (abld build no siempre rehace el makefile despues de tocar el .mmp)
call perl -S ABLD.PL "%~p0group\\" makefile armv5 || goto error
rem (abld sale con 0 aunque falle: se mira su salida)
call perl -S ABLD.PL "%~p0group\\" build armv5 urel > "%TEMP%\segaw3d_abld.log" 2>&1
type "%TEMP%\segaw3d_abld.log"
findstr /c:"Error:" /c:"ERROR:" /c:"make: ***" "%TEMP%\segaw3d_abld.log" > nul && goto error
echo.
echo OK: C:\epoc32\release\armv5\urel\segaw3d.exe
exit /b 0
:error
echo.
echo FALLO LA COMPILACION
exit /b 1
