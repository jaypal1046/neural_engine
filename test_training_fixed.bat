@echo off
echo ============================================================
echo Testing Fixed Training (Race Condition Fix Applied)
echo ============================================================
echo.

echo This will test training on 100 sequences (2-3 minutes)
echo to verify loss DECREASES (not increases)
echo.
pause

REM Create tiny test dataset
powershell -Command "Get-Content wiki_training_combined.txt -TotalCount 100 | Out-File -Encoding UTF8 test_100.txt"

echo.
echo Starting test training...
echo WATCH: Loss should DECREASE each epoch
echo   Good: 6.0 → 5.0 → 4.0 → 3.5 → 3.0
echo   Bad:  6.0 → 7.0 → 8.0 (STOP if this happens!)
echo.

bin\neural_engine.exe train_transformer test_100.txt 3 0.001 16

echo.
echo ============================================================
echo Test Complete!
echo.
echo Check the results above:
echo   - Did loss DECREASE? (Good ✓)
echo   - Did loss INCREASE? (Still broken ✗)
echo.
echo If loss decreased: Ready for full training!
echo If loss increased: Need more debugging
echo ============================================================
