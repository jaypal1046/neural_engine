@echo off
echo ============================================================
echo AIZip Neural Engine - Optimized Multi-threaded Build
echo ============================================================
echo.

REM Record start time
set START_TIME=%TIME%

echo [1/5] Detecting CPU cores...
for /f "tokens=2 delims==" %%I in ('wmic cpu get NumberOfCores /value ^| find "="') do set CPU_CORES=%%I
set /a PARALLEL_JOBS=%CPU_CORES% * 2
echo        CPU Cores: %CPU_CORES%
echo        Parallel Jobs: %PARALLEL_JOBS%
echo.

echo [2/5] Preparing build environment...
cd src
if exist ..\bin\neural_engine.exe (
    echo        Backing up existing binary...
    move /Y ..\bin\neural_engine.exe ..\bin\neural_engine.exe.bak >nul 2>&1
)
echo        Ready
echo.

echo [3/5] Compiling source files...
echo        Optimization: -O3 (maximum speed)
echo        SIMD: SSE2 + AVX2 + F16C
echo        Threading: OpenMP + pthreads
echo        Features: Smart Brain, Mixed Precision, Flash Attention
echo.

REM Build with all optimizations
g++ -O3 -std=c++17 -Wall ^
    -march=native -msse2 -mavx2 -mf16c ^
    -fopenmp ^
    -DINCLUDE_SMART_BRAIN ^
    -I../include ^
    -o ../bin/neural_engine.exe ^
    neural_engine.cpp ^
    compressor.cpp ^
    lz77.cpp ^
    huffman.cpp ^
    ans.cpp ^
    bwt.cpp ^
    bwt_simd.cpp ^
    ppm.cpp ^
    cmix.cpp ^
    knowledge_manager.cpp ^
    web_fetcher.cpp ^
    html_parser.cpp ^
    vector_index.cpp ^
    persistent_mixer.cpp ^
    compressed_knowledge.cpp ^
    block_access.cpp ^
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
    precision_utils.cpp ^
    kv_cache.cpp ^
    mixed_precision.cpp ^
    mixed_precision_commands.cpp ^
    data_loader.cpp ^
    tensor_ops.cpp ^
    flash_attention.cpp ^
    self_learning.cpp ^
    self_learning_bridge.cpp ^
    quantize_commands.cpp ^
    quantization.cpp ^
    rlhf.cpp ^
    advanced_reasoning.cpp ^
    -lwinhttp -lws2_32 -lgomp -pthread

if errorlevel 1 (
    echo.
    echo [ERROR] Compilation failed!
    cd ..
    exit /b 1
)

echo        Compilation successful
echo.

echo [4/5] Verifying build...
cd ..
if exist bin\neural_engine.exe (
    for %%A in (bin\neural_engine.exe) do set SIZE=%%~zA
    set /a SIZE_MB=!SIZE! / 1048576
    echo        Binary size: !SIZE! bytes (~!SIZE_MB! MB)
    echo        Build verified successfully
) else (
    echo [ERROR] Binary not found!
    exit /b 1
)
echo.

echo [5/5] Build complete!
REM Calculate build time
set END_TIME=%TIME%
echo        Start: %START_TIME%
echo        End:   %END_TIME%
echo.

REM Calculate time difference (simplified)
for /f "tokens=1-3 delims=:." %%a in ("%START_TIME%") do (
    set /a START_SEC=%%a*3600 + %%b*60 + %%c
)
for /f "tokens=1-3 delims=:." %%a in ("%END_TIME%") do (
    set /a END_SEC=%%a*3600 + %%b*60 + %%c
)
set /a ELAPSED=END_SEC - START_SEC
if %ELAPSED% LSS 0 set /a ELAPSED=ELAPSED + 86400

echo        Build time: %ELAPSED% seconds
echo.

REM Test the binary
echo Testing neural engine...
bin\neural_engine.exe --version
if errorlevel 1 (
    echo [WARNING] Binary test failed
) else (
    echo [SUCCESS] Neural engine ready!
)

echo.
echo ============================================================
echo Build Summary:
echo   - Multi-threading: OpenMP enabled
echo   - SIMD: AVX2 + SSE2
echo   - Optimization: -O3 -march=native
echo   - Binary: bin/neural_engine.exe
echo ============================================================
