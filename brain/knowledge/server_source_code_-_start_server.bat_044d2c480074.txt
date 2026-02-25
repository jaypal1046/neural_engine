@echo off
title Neural Studio V10 Server
echo.
echo ========================================
echo Starting Neural Studio V10 Server
echo ========================================
echo.
echo Server will auto-stop when desktop app closes.
echo.

REM Start Python server
python main.py

REM When python exits (Ctrl+C or app closes), this continues
echo.
echo Server stopped.
pause
