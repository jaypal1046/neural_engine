#!/bin/bash
set -e

echo "========================================"
echo "Building Smart Brain C++ Engine"
echo "========================================"
echo

cd src

echo "[1/2] Compiling smart_brain..."
g++ -O3 -std=c++17 -Wall -march=native \
    -I../include \
    -o ../bin/smart_brain \
    smart_brain.cpp \
    knowledge_manager.cpp \
    web_fetcher.cpp \
    html_parser.cpp \
    vector_index.cpp \
    persistent_mixer.cpp \
    compressor.cpp \
    lz77.cpp \
    huffman.cpp \
    ans.cpp \
    bwt.cpp \
    ppm.cpp \
    cmix.cpp

echo
echo "[2/2] Compiling myzip..."
g++ -O3 -std=c++17 -Wall -march=native \
    -I../include \
    -o ../bin/myzip \
    main.cpp \
    compressor.cpp \
    lz77.cpp \
    huffman.cpp \
    ans.cpp \
    bwt.cpp \
    ppm.cpp \
    cmix.cpp

cd ..

echo
echo "========================================"
echo "SUCCESS! Built:"
echo "  bin/smart_brain  (AI Knowledge Engine)"
echo "  bin/myzip        (Compression Tool)"
echo "========================================"
echo
echo "Test it:"
echo "  ./bin/smart_brain status"
echo "  ./bin/smart_brain learn https://en.wikipedia.org/wiki/Data_compression"
echo "  ./bin/smart_brain ask 'What is compression?'"
echo
