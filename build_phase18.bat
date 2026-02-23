@echo off
echo ========================================
echo Building Phase 18: Real Tokenizer + Embeddings
echo ========================================
echo.

set SRC_DIR=src
set INC_DIR=include
set TOOLS_DIR=tools
set BIN_DIR=bin

if not exist %BIN_DIR% mkdir %BIN_DIR%

echo [1/2] Compiling training tool (train_language_model.exe)...
g++ -O3 -std=c++17 -Wall -march=native ^
    -I%INC_DIR% ^
    %TOOLS_DIR%\train_language_model.cpp ^
    %SRC_DIR%\bpe_tokenizer.cpp ^
    %SRC_DIR%\real_embeddings.cpp ^
    -o %BIN_DIR%\train_language_model.exe

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Compilation failed!
    exit /b 1
)

echo.
echo [2/2] Testing...
echo Sample text > test_corpus.txt
echo The quick brown fox jumps over the lazy dog. >> test_corpus.txt
echo Machine learning is a subset of artificial intelligence. >> test_corpus.txt
echo Natural language processing enables computers to understand text. >> test_corpus.txt

echo.
echo Running quick test...
%BIN_DIR%\train_language_model.exe test_corpus.txt models\

del test_corpus.txt

echo.
echo ========================================
echo SUCCESS! Phase 18 complete.
echo ========================================
echo.
echo Built:
echo   bin\train_language_model.exe
echo.
echo Models saved to:
echo   models\tokenizer.bin
echo   models\embeddings.bin
echo.
echo Next: Train on real corpus (Wikipedia, books, etc.)
echo   train_language_model.exe your_corpus.txt models\
echo.
