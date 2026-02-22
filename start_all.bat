@echo off
echo ========================================
echo Starting Neural Studio V10
echo ========================================
echo.

REM Check if neural_engine.exe exists
if not exist "bin\neural_engine.exe" (
    echo ERROR: neural_engine.exe not found!
    echo.
    echo Please build it first:
    echo   build_smart_brain.bat
    echo.
    pause
    exit /b 1
)

echo [1/2] Starting Python Server...
start "Neural Studio Server" cmd /k "cd server && python main.py"

REM Wait 3 seconds for server to start
timeout /t 3 /nobreak >nul

echo [2/2] Starting Desktop App...
cd desktop_app
call npm run dev

REM When desktop app closes, kill the server window
echo.
echo Desktop app closed. Stopping server...
taskkill /FI "WindowTitle eq Neural Studio Server*" /T /F >nul 2>&1

echo.
echo ========================================
echo Neural Studio stopped.
echo ========================================
pause
