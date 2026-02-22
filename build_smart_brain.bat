@echo off
echo ========================================
echo Building Neural Studio C++ Engines
echo ========================================
echo.

cd src

echo [1/3] Compiling neural_engine.exe (with Smart Brain)...
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
echo   bin/neural_engine.exe  (AI + Smart Brain + Math)
echo   bin/myzip.exe          (Compression Tool)
echo ========================================
echo.
echo Test it:
echo   bin\neural_engine.exe status
echo   bin\neural_engine.exe learn https://en.wikipedia.org/wiki/Data_compression
echo   bin\neural_engine.exe ask "What is compression?"
echo   bin\neural_engine.exe math "2 + 3 * 4"
echo.
