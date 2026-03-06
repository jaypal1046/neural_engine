@echo off
echo ================================================
echo Building Tokenizer Creator
echo ================================================

g++ -O3 -std=c++17 -Wall ^
    -I./include ^
    -o ./bin/create_tokenizer.exe ^
    create_tokenizer.cpp ^
    src/bpe_tokenizer.cpp ^
    -lstdc++fs

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful!
    echo.
    echo Executable: bin/create_tokenizer.exe
    echo.
) else (
    echo.
    echo Build FAILED!
    echo.
    exit /b 1
)
