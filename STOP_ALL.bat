@echo off
REM Stop All Neural Studio Processes
echo ========================================
echo Stopping Neural Studio AI System
echo ========================================
echo.

echo [1/3] Stopping Python server (port 8001)...
taskkill /F /IM python.exe /FI "WINDOWTITLE eq Neural Engine Server*" > nul 2>&1
for /f "tokens=5" %%a in ('netstat -aon ^| findstr :8001') do taskkill /F /PID %%a > nul 2>&1
echo [OK] Python server stopped

echo [2/3] Stopping Desktop App (port 5173)...
taskkill /F /IM node.exe /FI "WINDOWTITLE eq Desktop App*" > nul 2>&1
for /f "tokens=5" %%a in ('netstat -aon ^| findstr :5173') do taskkill /F /PID %%a > nul 2>&1
echo [OK] Desktop App stopped

echo [3/3] Closing terminal windows...
taskkill /F /FI "WINDOWTITLE eq Neural Engine Server*" > nul 2>&1
taskkill /F /FI "WINDOWTITLE eq Desktop App*" > nul 2>&1
echo [OK] Terminal windows closed

echo.
echo ========================================
echo All processes stopped successfully!
echo ========================================
echo.
pause
