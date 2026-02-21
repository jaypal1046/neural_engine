CXX      = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -march=native -Iinclude

TARGET   = bin/myzip[EXT]
SRCS     = src/main.cpp src/compressor.cpp src/lz77.cpp src/huffman.cpp src/ans.cpp src/bwt.cpp src/ppm.cpp src/cmix.cpp
OBJS     = $(patsubst src/%.cpp, obj/%.o, $(SRCS))

# Handle Windows vs Max/Linux executable extension
ifeq ($(OS),Windows_NT)
	TARGET = bin/myzip.exe
else
	TARGET = bin/myzip
endif

all: prep $(TARGET)

prep:
	@mkdir -p obj bin

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf obj/ $(TARGET)
