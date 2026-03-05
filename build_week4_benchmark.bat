@echo off
echo ========================================
echo Building Week 4 Integration Benchmark
echo ========================================
echo.

cd test

echo [1/1] Building benchmark_week4.exe...
g++ -O3 -std=c++17 -march=native -msse2 -mavx2 -I../include ^
    -o ../bin/benchmark_week4.exe ^
    benchmark_week4_integration.cpp ^
    ../src/tensor_ops.cpp ../src/tensor_ops_advanced.cpp ^
    ../src/flash_attention.cpp ../src/kv_cache.cpp

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
echo Executable: bin\benchmark_week4.exe
echo.
echo Run: bin\benchmark_week4.exe
echo.
pause
