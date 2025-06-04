CXX = g++
CXXFLAGS = -std=c++17 -O3 -Isrc

main: src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o main

clean:
	rm -f main

.PHONY: clean 