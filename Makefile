CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Iinclude
SRC = src/main.cpp src/huffman.cpp src/archive.cpp
TARGET = compressor

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
