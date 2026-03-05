@echo off
REM Build Unigram Tokenizer Test

echo ========================================
echo Building Unigram Tokenizer Test
echo ========================================
echo.

cd src

echo Compiling tokenizer test...
g++ -O3 -std=c++17 -Wall ^
    -I../include ^
    -o ../bin/test_unigram_tokenizer.exe ^
    ../test/test_unigram_tokenizer.cpp ^
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
echo Run: bin\test_unigram_tokenizer.exe
echo.
echo This will test:
echo   - Tokenizer training on small corpus
echo   - Encode/decode functionality
echo   - Batch processing
echo   - Special tokens
echo   - Save/load model
echo.

pause
