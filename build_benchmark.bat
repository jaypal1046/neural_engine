@echo off
REM Build tensor operations benchmark

echo ========================================
echo Building Tensor Operations Benchmark
echo ========================================
echo.

cd src

echo Compiling tensor_ops benchmark...
g++ -O3 -std=c++17 -Wall -march=native -msse2 -mavx2 ^
    -I../include ^
    -o ../bin/benchmark_tensor_ops.exe ^
    ../test/benchmark_tensor_ops.cpp ^
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
echo Run: bin\benchmark_tensor_ops.exe
echo.

pause
