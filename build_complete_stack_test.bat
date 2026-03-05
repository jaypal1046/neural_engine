@echo off
REM Build Complete Stack Integration Test (Week 1 + Week 2 + Week 3)

echo ========================================
echo Building Complete Stack Integration Test
echo ========================================
echo.

cd src

echo Compiling complete stack test...
g++ -O3 -std=c++17 -Wall -march=native -msse2 -mavx2 ^
    -I../include ^
    -o ../bin/test_complete_stack.exe ^
    ../test/test_complete_stack.cpp ^
    tensor_ops.cpp ^
    tensor_ops_advanced.cpp ^
    quantization.cpp ^
    kv_cache.cpp ^
    flash_attention.cpp ^
    mistral_attention.cpp ^
    qwen_attention.cpp ^
    mixed_precision.cpp ^
    gradient_checkpoint.cpp ^
    unigram_tokenizer.cpp

if errorlevel 1 (
    echo ERROR: Build failed!
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo ========================================
echo Build Complete!
echo ========================================
echo.
echo Run: bin\test_complete_stack.exe
echo.
echo This will test ALL Week 1 + Week 2 + Week 3 systems:
echo.
echo   WEEK 1 - Core Performance:
echo     • SIMD Tensor Operations
echo     • 4-bit Quantization
echo     • KV-Cache + GQA
echo.
echo   WEEK 2 - Architecture Upgrades:
echo     • Flash Attention v2
echo     • Mistral Sliding Window
echo     • Qwen Dual Attention
echo.
echo   WEEK 3 - Training Optimizations:
echo     • Mixed Precision Training
echo     • Gradient Checkpointing
echo     • Unigram Tokenizer
echo.

pause
