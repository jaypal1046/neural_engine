CXX      = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -march=native
TARGET   = myzip.exe
SRCS     = main.cpp compressor.cpp lz77.cpp huffman.cpp ans.cpp bwt.cpp
OBJS     = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

# Dependencies
main.o:       main.cpp compressor.h
compressor.o: compressor.cpp compressor.h lz77.h huffman.h bit_io.h bwt.h
lz77.o:       lz77.cpp lz77.h
huffman.o:    huffman.cpp huffman.h bit_io.h
bwt.o:        bwt.cpp bwt.h
