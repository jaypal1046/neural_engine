@echo off
echo ============================================================
echo Multi-Threading Performance Test
echo ============================================================
echo.

REM Create test file if it doesn't exist
if not exist test_data.txt (
    echo Creating test file...
    echo This is a test file for benchmarking the multi-threaded neural engine. > test_data.txt
    for /L %%i in (1,1,100) do (
        echo The quick brown fox jumps over the lazy dog. Machine learning is amazing! >> test_data.txt
    )
)

echo [1/3] Testing Compression Speed (BWT Multi-threading)
echo -------------------------------------------------------
set START_TIME=%TIME%

bin\neural_engine.exe compress test_data.txt test_output.aiz --best >nul 2>&1
if errorlevel 1 (
    echo [FAIL] Compression failed
) else (
    echo [PASS] Compression succeeded
)

set END_TIME=%TIME%
echo Start: %START_TIME%
echo End:   %END_TIME%
echo.

echo [2/3] Testing Decompression Speed
echo -------------------------------------------------------
set START_TIME=%TIME%

bin\neural_engine.exe decompress test_output.aiz test_output_verify.txt >nul 2>&1
if errorlevel 1 (
    echo [FAIL] Decompression failed
) else (
    echo [PASS] Decompression succeeded
)

set END_TIME=%TIME%
echo Start: %START_TIME%
echo End:   %END_TIME%
echo.

echo [3/3] Testing AI Inference Speed (Transformer Multi-threading)
echo -------------------------------------------------------
echo Question: "What is AI?"
set START_TIME=%TIME%

bin\neural_engine.exe ai_ask "What is AI?"
if errorlevel 1 (
    echo [FAIL] AI inference failed
) else (
    echo [PASS] AI inference succeeded
)

set END_TIME=%TIME%
echo Start: %START_TIME%
echo End:   %END_TIME%
echo.

REM Cleanup
del test_output.aiz >nul 2>&1
del test_output_verify.txt >nul 2>&1

echo ============================================================
echo Test Complete!
echo ============================================================
echo.
echo Note: For accurate benchmarks, run multiple times and average.
echo Expected improvements with multi-threading:
echo   - Compression: 4-8x faster on multi-core CPUs
echo   - AI Inference: 4-7x faster on matrix operations
echo.
