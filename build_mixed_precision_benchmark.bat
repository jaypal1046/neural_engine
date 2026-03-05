@echo off
REM Build Mixed Precision Training Benchmark

echo ========================================
echo Building Mixed Precision Benchmark
echo ========================================
echo.

cd src

echo Compiling mixed precision benchmark...
g++ -O3 -std=c++17 -Wall ^
    -I../include ^
    -o ../bin/benchmark_mixed_precision.exe ^
    ../test/benchmark_mixed_precision.cpp ^
    mixed_precision.cpp

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
echo Run: bin\benchmark_mixed_precision.exe
echo.
echo This will test:
echo   - FP16/BF16 conversion accuracy
echo   - Memory savings (2x less)
echo   - Dynamic loss scaling
echo   - Gradient overflow detection
echo   - Mixed precision optimizer
echo.

pause
