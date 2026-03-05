@echo off
REM Build Full Stack Integration Test (Week 1 + Week 2)

echo ========================================
echo Building Full Stack Integration Test
echo ========================================
echo.

cd src

echo Compiling full stack test...
g++ -O3 -std=c++17 -Wall -march=native -msse2 -mavx2 ^
    -I../include ^
    -o ../bin/test_full_stack.exe ^
    ../test/test_full_stack.cpp ^
    tensor_ops.cpp ^
    tensor_ops_advanced.cpp ^
    quantization.cpp ^
    kv_cache.cpp ^
    flash_attention.cpp ^
    mistral_attention.cpp ^
    qwen_attention.cpp

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
echo Run: bin\test_full_stack.exe
echo.
echo This will test ALL Week 1 + Week 2 optimizations:
echo   - SIMD Tensor Operations
echo   - 4-bit Quantization
echo   - KV-Cache + GQA
echo   - Flash Attention v2
echo   - Sliding Window Attention
echo   - Dual Attention Pattern
echo.

pause
