@echo off
REM Build quantization benchmark

echo ========================================
echo Building Quantization Benchmark
echo ========================================
echo.

cd src

echo Compiling quantization benchmark...
g++ -O3 -std=c++17 -Wall -march=native -msse2 -mavx2 ^
    -I../include ^
    -o ../bin/benchmark_quantization.exe ^
    ../test/benchmark_quantization.cpp ^
    quantization.cpp

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
echo Run: bin\benchmark_quantization.exe
echo.

pause
