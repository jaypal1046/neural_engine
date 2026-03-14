@echo off
echo ============================================================
echo Optimized Training Configuration
echo ============================================================
echo.

REM Check if wiki_training_combined.txt exists
if not exist wiki_training_combined.txt (
    echo ERROR: wiki_training_combined.txt not found!
    exit /b 1
)

echo [1/3] Creating optimized training dataset...
echo.
echo Options:
echo   1. Ultra-fast (1,000 sequences, ~30 min)
echo   2. Recommended (2,000 sequences, ~2 hours)
echo   3. High-quality (5,000 sequences, ~4 hours)
echo.
set /p CHOICE="Select option (1-3): "

if "%CHOICE%"=="1" (
    set SEQUENCES=1000
    set EPOCHS=5
    set LR=0.002
    set BATCH=32
    set FILENAME=wiki_1k.txt
    set TIME=30 minutes
) else if "%CHOICE%"=="2" (
    set SEQUENCES=2000
    set EPOCHS=5
    set LR=0.001
    set BATCH=32
    set FILENAME=wiki_2k.txt
    set TIME=2 hours
) else if "%CHOICE%"=="3" (
    set SEQUENCES=5000
    set EPOCHS=5
    set LR=0.0005
    set BATCH=32
    set FILENAME=wiki_5k.txt
    set TIME=4 hours
) else (
    echo Invalid choice. Using default (option 2)
    set SEQUENCES=2000
    set EPOCHS=5
    set LR=0.001
    set BATCH=32
    set FILENAME=wiki_2k.txt
    set TIME=2 hours
)

echo.
echo Configuration:
echo   Sequences: %SEQUENCES%
echo   Epochs: %EPOCHS%
echo   Learning Rate: %LR%
echo   Batch Size: %BATCH%
echo   Expected Time: %TIME%
echo.

echo Creating dataset using PowerShell...
powershell -Command "Get-Content wiki_training_combined.txt -TotalCount %SEQUENCES% | Out-File -Encoding UTF8 %FILENAME%"

if errorlevel 1 (
    echo ERROR: Failed to create dataset
    exit /b 1
)

echo ✓ Created %FILENAME% (%SEQUENCES% sequences)
echo.

echo [2/3] Starting training...
echo.
echo IMPORTANT: Watch for DECREASING loss
echo   Good training: Loss goes down each epoch
echo   Bad training: Loss stays same or goes up
echo.
echo Press any key to start training...
pause >nul

echo.
echo ============================================================
echo Training Started
echo ============================================================
echo.

bin\neural_engine.exe train_transformer %FILENAME% %EPOCHS% %LR% %BATCH%

echo.
echo [3/3] Training complete!
echo.
echo Next steps:
echo   - Check if loss DECREASED over epochs
echo   - Test the model with: bin\neural_engine.exe ai_ask "test question"
echo   - If loss didn't decrease, try higher learning rate
echo.
echo ============================================================
