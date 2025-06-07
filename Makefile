CXX = g++
CXXFLAGS = -std=c++20 -Wall -Isrc -Isrc/core -Isrc/algo -Isrc/tests -Isrc/ds -fopenmp -MMD -MP

# Object files and dependency files
MAIN_OBJ = src/main.o
GRAPH_GEN_OBJ = src/tests/graph_generator.o
BENCHMARK_OBJ = src/tests/benchmark_tool.o
# DELTA_BENCH_OBJ = src/tests/delta_stepping_benchmark.o

# Dependency files
DEPS = $(MAIN_OBJ:.o=.d) $(GRAPH_GEN_OBJ:.o=.d) $(BENCHMARK_OBJ:.o=.d)

# Include dependency files if they exist
-include $(DEPS)

main: $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) $(MAIN_OBJ) -o main

graph_generator: $(GRAPH_GEN_OBJ)
	$(CXX) $(CXXFLAGS) $(GRAPH_GEN_OBJ) -o graph_generator

benchmark: $(BENCHMARK_OBJ)
	$(CXX) $(CXXFLAGS) $(BENCHMARK_OBJ) -o benchmark


# Pattern rule for compiling object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

all: main graph_generator benchmark

clean:
	rm -f main graph_generator benchmark
	rm -f src/*.o src/*.d src/tests/*.o src/tests/*.d

.PHONY: clean all 