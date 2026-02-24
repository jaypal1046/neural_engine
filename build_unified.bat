@echo off
REM Build unified neural_engine.exe with ALL features
echo ========================================
echo Building Unified Neural Engine
echo ========================================
echo.

cd src

echo [1/1] Building unified neural_engine.exe (ALL features)...
g++ -O3 -std=c++17 -Wall -march=native -msse2 -mavx2 ^
    -DINCLUDE_SMART_BRAIN -I../include ^
    -o ../bin/neural_engine.exe ^
    neural_engine_unified.cpp ^
    main.cpp compressor.cpp ^
    neural_engine.cpp knowledge_manager.cpp web_fetcher.cpp html_parser.cpp ^
    vector_index.cpp persistent_mixer.cpp compressed_knowledge.cpp block_access.cpp ^
    lz77.cpp huffman.cpp ans.cpp bwt.cpp bwt_simd.cpp ppm.cpp cmix.cpp ^
    word_tokenizer.cpp word_ppm.cpp embedding_trainer.cpp rag_engine.cpp ^
    conversation_memory.cpp reasoning_engine.cpp bpe_tokenizer.cpp real_embeddings.cpp ^
    mini_transformer.cpp optimizer.cpp loss.cpp transformer_gradients.cpp ^
    test_block_access.cpp ^
    -lwinhttp -lws2_32 -pthread

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
echo Unified executable: bin\neural_engine.exe
echo.
echo This ONE executable can:
echo   - Compress/decompress files
echo   - Run AI features (knowledge, RAG, transformer)
echo   - Test block access
echo   - Train models
echo.
echo Examples:
echo   neural_engine compress file.txt --best
echo   neural_engine knowledge_query capabilities "What can you do?"
echo   neural_engine ai_ask "Explain BWT compression"
echo   neural_engine test_block file.aiz
echo.
pause
