@echo off
python -V >nul 2>&1
if errorlevel 1 (
  echo Python not runnable; skipping data validation.
  exit /b 0
)
python tools\validate_data.py
exit /b %ERRORLEVEL%
