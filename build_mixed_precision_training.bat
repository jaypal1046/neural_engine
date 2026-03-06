@echo off
REM Build Mixed Precision Training Test (Week 9 Day 5)

echo ========================================
echo Building Mixed Precision Training Test
echo ========================================
echo.

echo Compiling test_mixed_precision_training.cpp...
g++ -O3 -std=c++17 -Wall -mf16c -mavx2 ^
    -I./include ^
    -o ./bin/test_mixed_precision_training.exe ^
    test_mixed_precision_training.cpp ^
    src/mini_transformer.cpp ^
    src/bpe_tokenizer.cpp ^
    src/real_embeddings.cpp ^
    src/optimizer.cpp ^
    src/loss.cpp ^
    src/transformer_gradients.cpp ^
    src/tensor_ops.cpp ^
    src/tensor_ops_advanced.cpp ^
    src/flash_attention.cpp ^
    src/kv_cache.cpp ^
    src/precision_utils.cpp ^
    src/mixed_precision.cpp

if errorlevel 1 (
    echo.
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build Complete!
echo ========================================
echo.
echo Run: bin\test_mixed_precision_training.exe
echo.
echo This will test:
echo   - FP32 training (baseline)
echo   - FP16 training (with loss scaling)
echo   - BF16 training (no loss scaling)
echo   - Loss convergence for all modes
echo   - Accuracy comparison vs FP32
echo   - Training speedup measurement
echo.

pause
