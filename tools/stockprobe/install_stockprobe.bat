@echo off
rem Put the stock SNES cores in cubegm\cores\real and stockprobe in their place.
rem Run from the SD card root, with stockprobe.so next to this file.
set CORES=%~dp0cubegm\cores
if not exist "%~dp0stockprobe.so" ( echo stockprobe.so is not next to this file. & pause & exit /b 1 )
if not exist "%CORES%" ( echo No cubegm\cores folder here: run this from the SD card root. & pause & exit /b 1 )
if not exist "%CORES%\real" mkdir "%CORES%\real"
for %%c in (libemu_sfc.so libemu_snes9x.so libemu_snes9x2010.so) do (
  if exist "%CORES%\%%c" if not exist "%CORES%\real\%%c" move "%CORES%\%%c" "%CORES%\real\%%c" >nul
  if exist "%CORES%\real\%%c" copy /y "%~dp0stockprobe.so" "%CORES%\%%c" >nul
)
echo Installed. Play, then send logs\stockprobe.log. Run uninstall_stockprobe.bat to undo.
pause
