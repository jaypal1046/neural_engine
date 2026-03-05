@echo off
REM Quick Start - Neural Studio AI System
echo ========================================
echo Starting Neural Studio AI System
echo ========================================
echo.

REM Kill zombie processes first
echo [1/5] Cleaning up zombie processes...
taskkill /F /IM python.exe /FI "WINDOWTITLE eq Neural Engine Server*" > nul 2>&1
taskkill /F /IM node.exe /FI "WINDOWTITLE eq Desktop App*" > nul 2>&1

REM Kill processes using ports 8001 and 5173
for /f "tokens=5" %%a in ('netstat -aon ^| findstr :8001') do taskkill /F /PID %%a > nul 2>&1
for /f "tokens=5" %%a in ('netstat -aon ^| findstr :5173') do taskkill /F /PID %%a > nul 2>&1

timeout /t 2 /nobreak > nul
echo [OK] Zombie processes cleaned

REM Build if needed
if not exist "bin\neural_engine.exe" (
    echo [2/5] Building system first...
    call build_unified.bat
) else (
    echo [2/5] Neural engine already built
)

REM Create brain directories
echo [3/5] Creating brain directories...
if not exist "brain\feedback" mkdir "brain\feedback"
if not exist "brain\training" mkdir "brain\training"
if not exist "brain\self_learning" mkdir "brain\self_learning"
if not exist "brain\feedback\archive" mkdir "brain\feedback\archive"

REM Start Python server
echo [4/5] Starting Python server on port 8001...
start "Neural Engine Server" cmd /k "cd server && python main.py"

timeout /t 3 /nobreak > nul

REM Start Desktop App
echo [5/5] Starting Desktop App on port 5173...
start "Desktop App" cmd /k "cd desktop_app && npm run dev"

timeout /t 8 /nobreak > nul

REM Open Chrome
echo.
echo ========================================
echo System Started Successfully!
echo ========================================
echo.
echo Python Server: http://127.0.0.1:8001
echo Desktop App:   http://localhost:5173
echo API Docs:      http://127.0.0.1:8001/docs
echo.
echo Opening Chrome in 2 seconds...
timeout /t 2 /nobreak > nul
start chrome http://localhost:5173

echo.
echo See TESTING_GUIDE.md for test instructions.
echo.
pause
