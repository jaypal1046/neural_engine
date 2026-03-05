@echo off
REM Build KV-cache benchmark

echo ========================================
echo Building KV-Cache Benchmark
echo ========================================
echo.

cd src

echo Compiling KV-cache benchmark...
g++ -O3 -std=c++17 -Wall -march=native -msse2 -mavx2 ^
    -I../include ^
    -o ../bin/benchmark_kv_cache.exe ^
    ../test/benchmark_kv_cache.cpp ^
    kv_cache.cpp ^
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
echo Run: bin\benchmark_kv_cache.exe
echo.

pause
