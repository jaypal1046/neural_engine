@echo off
REM Build LLM Training System

echo ========================================
echo Building LLM Training System
echo ========================================
echo.

echo Compiling train_llm.cpp...
g++ -O3 -std=c++17 -Wall -mf16c -mavx2 ^
    -I./include ^
    -o ./bin/train_llm.exe ^
    train_llm.cpp ^
    src/data_loader.cpp ^
    src/mini_transformer.cpp ^
    src/bpe_tokenizer.cpp ^
    src/real_embeddings.cpp ^
    src/optimizer.cpp ^
    src/loss.cpp ^
    src/transformer_gradients.cpp ^
    src/tensor_ops.cpp ^
    src/tensor_ops_advanced.cpp ^
    src/flash_attention.cpp ^
    src/kv_cache.cpp ^
    src/precision_utils.cpp ^
    src/mixed_precision.cpp

if errorlevel 1 (
    echo.
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build Complete!
echo ========================================
echo.
echo Run: bin\train_llm.exe --help
echo.
echo Quick Start:
echo   1. Get training data (Wikipedia sample):
echo      Download small corpus to wiki_clean.txt
echo.
echo   2. Start training:
echo      bin\train_llm.exe --corpus wiki_clean.txt --epochs 5 --batch 4
echo.
echo   3. Training uses FP16 mixed precision (6.35x faster!)
echo.
echo   4. Wait for model to train (saves checkpoints)
echo.
echo   5. Test trained model with inference engine
echo.

pause
