@echo off
REM Test AI System - Phase H-I Complete
echo ========================================
echo Testing Neural Studio AI System
echo ========================================
echo.

REM Check if neural_engine.exe exists
if not exist "bin\neural_engine.exe" (
    echo [ERROR] bin\neural_engine.exe not found!
    echo Please run build_unified.bat first.
    pause
    exit /b 1
)

echo [1/6] Testing neural_engine.exe basic functionality...
bin\neural_engine.exe --help > nul 2>&1
if errorlevel 1 (
    echo [FAIL] neural_engine.exe not working
    pause
    exit /b 1
) else (
    echo [PASS] neural_engine.exe is functional
)

echo.
echo [2/6] Testing AI ask command...
bin\neural_engine.exe ai_ask "What is compression?" > test_output.txt 2>&1
if errorlevel 1 (
    echo [FAIL] ai_ask command failed
    type test_output.txt
) else (
    echo [PASS] ai_ask command works
    echo Response preview:
    type test_output.txt | findstr /C:"compression" /C:"data" /C:"algorithm" > nul
    if errorlevel 1 (
        echo [WARN] Response may be generic
    ) else (
        echo [PASS] Response contains relevant keywords
    )
)

echo.
echo [3/6] Checking Python server dependencies...
python --version > nul 2>&1
if errorlevel 1 (
    echo [FAIL] Python not found in PATH
    pause
    exit /b 1
) else (
    echo [PASS] Python is available
)

echo.
echo [4/6] Checking FastAPI installation...
python -c "import fastapi" > nul 2>&1
if errorlevel 1 (
    echo [WARN] FastAPI not installed
    echo Installing FastAPI...
    pip install fastapi uvicorn
) else (
    echo [PASS] FastAPI is installed
)

echo.
echo [5/6] Creating brain directories...
if not exist "brain\feedback" mkdir "brain\feedback"
if not exist "brain\training" mkdir "brain\training"
if not exist "brain\self_learning" mkdir "brain\self_learning"
echo [PASS] Brain directories created

echo.
echo [6/6] Starting servers...
echo.
echo ========================================
echo READY TO TEST!
echo ========================================
echo.
echo Next Steps:
echo 1. Start Python server:
echo    cd server
echo    python main.py
echo.
echo 2. Start Desktop App (in another terminal):
echo    cd desktop_app
echo    npm run dev
echo.
echo 3. Open Chrome at: http://localhost:5173
echo.
echo 4. Test AI features:
echo    - Open AI Chat (Brain icon)
echo    - Ask a question
echo    - Click thumbs up/down
echo    - Check AI Training Stats (TrendingUp icon)
echo.
echo 5. Test backend API directly:
echo    - http://127.0.0.1:8001/api/health
echo    - http://127.0.0.1:8001/docs (FastAPI docs)
echo.
pause
