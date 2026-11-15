@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"
echo ======================================================================
echo   Compilando MI AMP Bass Edition (Windows x64 VST3 + Standalone)
echo ======================================================================
where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] No se encontro CMake. Podes compilar automatico en GitHub Actions!
  pause
  exit /b 1
)
cmake -B build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :err
cmake --build build --config Release --parallel
if errorlevel 1 goto :err

where makensis >nul 2>nul
if not errorlevel 1 (
  makensis installer_win64.nsi
  if errorlevel 1 goto :err
  echo.
  echo LISTO! Instalador generado: Instalar_MI_AMP_Windows_x64.exe
) else (
  echo.
  echo Compilacion lista en: build\MiAmp_artefacts\Release\
  echo (Instala NSIS si queres generar Instalar_MI_AMP_Windows_x64.exe localmente)
)
pause
exit /b 0
:err
echo.
echo Hubo un error al compilar.
pause
exit /b 1
