@echo off
echo ========================================
echo Building Unified AI Engine (All Phases)
echo ========================================
echo.

cd src

echo [1/3] Compiling neural_engine.exe (with Smart Brain + AI Phases 13-21)...
g++ -O3 -std=c++17 -Wall -march=native ^
    -DINCLUDE_SMART_BRAIN ^
    -I../include ^
    -o ../bin/neural_engine.exe ^
    neural_engine.cpp ^
    knowledge_manager.cpp ^
    web_fetcher.cpp ^
    html_parser.cpp ^
    vector_index.cpp ^
    persistent_mixer.cpp ^
    compressor.cpp ^
    lz77.cpp ^
    huffman.cpp ^
    ans.cpp ^
    bwt.cpp ^
    ppm.cpp ^
    cmix.cpp ^
    word_tokenizer.cpp ^
    word_ppm.cpp ^
    embedding_trainer.cpp ^
    rag_engine.cpp ^
    conversation_memory.cpp ^
    reasoning_engine.cpp ^
    bpe_tokenizer.cpp ^
    real_embeddings.cpp ^
    mini_transformer.cpp ^
    optimizer.cpp ^
    loss.cpp ^
    transformer_gradients.cpp ^
    -lwinhttp -lws2_32

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] neural_engine compilation failed!
    cd ..
    exit /b 1
)

echo.
echo [2/3] Compiling myzip.exe (compression tool)...
g++ -O3 -std=c++17 -Wall -march=native ^
    -I../include ^
    -o ../bin/myzip.exe ^
    main.cpp ^
    compressor.cpp ^
    lz77.cpp ^
    huffman.cpp ^
    ans.cpp ^
    bwt.cpp ^
    ppm.cpp ^
    cmix.cpp

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] myzip compilation failed!
    cd ..
    exit /b 1
)

echo.
echo [3/3] Removing old smart_brain.exe (merged into neural_engine)...
if exist ../bin/smart_brain.exe (
    del ../bin/smart_brain.exe
    echo   Deleted smart_brain.exe
) else (
    echo   smart_brain.exe not found (already removed)
)

cd ..

echo.
echo ========================================
echo SUCCESS! Built:
echo   bin/neural_engine.exe  (Complete Unified AI)
echo   bin/myzip.exe          (Compression Tool)
echo ========================================
echo.
echo Unified AI includes:
echo   - Smart Brain (learn/ask/status)
echo   - Word Model (5-gram PPM, BPE tokenizer)
echo   - Embeddings (Word2Vec, negative sampling)
echo   - RAG Engine (retrieval + generation)
echo   - Memory (conversation, feedback, learning)
echo   - Reasoning (chain-of-thought, verification)
echo   - Compression (CMIX, BWT, PPM, rANS)
echo   - Math Engine (eval, stats, entropy)
echo.
echo Test it:
echo   bin\neural_engine.exe ai_ask "What is BWT compression?"
echo   bin\neural_engine.exe rag_ask "Best compression for logs?"
echo   bin\neural_engine.exe word_generate "Once upon a time" 30
echo   bin\neural_engine.exe embed_analogy king man woman
echo   bin\neural_engine.exe reason "Why is CMIX better than gzip?"
echo.
