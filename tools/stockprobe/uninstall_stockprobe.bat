@echo off
rem Put the stock SNES cores back where they were.
set CORES=%~dp0cubegm\cores
for %%c in (libemu_sfc.so libemu_snes9x.so libemu_snes9x2010.so) do (
  if exist "%CORES%\real\%%c" (
    del "%CORES%\%%c" 2>nul
    move "%CORES%\real\%%c" "%CORES%\%%c" >nul
  )
)
rmdir "%CORES%\real" 2>nul
echo The stock cores are back.
pause
