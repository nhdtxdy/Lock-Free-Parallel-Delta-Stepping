CXX = g++
CXXFLAGS = -std=c++17 -Wall -O3 -Isrc -Isrc/core -Isrc/algo -Isrc/tests -Isrc/queues -Isrc/pools

main: src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o main

clean:
	rm -f main

.PHONY: clean 