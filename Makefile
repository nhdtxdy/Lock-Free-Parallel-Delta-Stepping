CXX = g++
CXXFLAGS = -std=c++17 -O3 -Isrc -Isrc/core -Isrc/algo -Isrc/tests

main: src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o main

clean:
	rm -f main

.PHONY: clean 