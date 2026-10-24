@echo off
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 (
  python "%~dp0instalar.py"
) else (
  py -3 "%~dp0instalar.py"
)
pause
