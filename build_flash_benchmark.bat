@echo off
REM Build Flash Attention benchmark

echo ========================================
echo Building Flash Attention Benchmark
echo ========================================
echo.

cd src

echo Compiling Flash Attention benchmark...
g++ -O3 -std=c++17 -Wall -march=native -msse2 -mavx2 ^
    -I../include ^
    -o ../bin/benchmark_flash_attention.exe ^
    ../test/benchmark_flash_attention.cpp ^
    flash_attention.cpp ^
    tensor_ops.cpp ^
    tensor_ops_advanced.cpp

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
echo Run: bin\benchmark_flash_attention.exe
echo.

pause
