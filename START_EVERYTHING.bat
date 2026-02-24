@echo off
echo ========================================
echo Starting Neural Studio with Trained AI
echo ========================================
echo.

echo [1/3] Killing old processes...
taskkill /F /IM python.exe 2>nul
taskkill /F /IM node.exe 2>nul
taskkill /F /IM electron.exe 2>nul
timeout /t 2 /nobreak >nul

echo [2/3] Starting Python server (port 8001)...
cd server
start "Python Server" cmd /k python main.py
timeout /t 3 /nobreak >nul

echo [3/3] Starting Desktop App...
cd ..\desktop_app
start "Desktop App" cmd /k npm run dev

echo.
echo ========================================
echo Everything Started!
echo ========================================
echo.
echo Python Server: http://localhost:8001
echo Desktop App: Will open automatically
echo.
echo Close this window anytime.
pause
