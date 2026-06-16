CXX = g++
CXXFLAGS = -std=c++17 -O2 -pthread -I../src -I../src/base
GPERF_INCLUDE = -I../Google_src/build -I../Google_src/src
GPERF_LIB = -L../Google_src/build -ltcmalloc_minimal -Wl,-rpath,../Google_src/build
SRCDIR = ../src
TC_SRCS = $(SRCDIR)/ThreadCache.cpp $(SRCDIR)/CentralCache.cpp $(SRCDIR)/PageCache.cpp

TARGETS = malloc_bench_mine malloc_bench_google malloc_bench_sys \
          binary_trees_mine binary_trees_google binary_trees_sys

all: $(TARGETS)

# Mine version: links TCMemAllocator only
malloc_bench_mine: malloc_bench.cpp run_benchmark.h run_benchmark.cc trivialre.h $(TC_SRCS)
	$(CXX) $(CXXFLAGS) -DMINE_ONLY -o $@ malloc_bench.cpp run_benchmark.cc $(TC_SRCS)

# Google version: links tcmalloc_minimal
malloc_bench_google: malloc_bench.cpp run_benchmark.h run_benchmark.cc trivialre.h
	$(CXX) $(CXXFLAGS) -DGOOGLE_ONLY $(GPERF_INCLUDE) -o $@ malloc_bench.cpp run_benchmark.cc $(GPERF_LIB)

# Sys version: plain system malloc
malloc_bench_sys: malloc_bench.cpp run_benchmark.h run_benchmark.cc trivialre.h
	$(CXX) $(CXXFLAGS) -DSYS_ONLY -o $@ malloc_bench.cpp run_benchmark.cc

# Binary trees - Mine
binary_trees_mine: binary_trees.cpp $(TC_SRCS)
	$(CXX) $(CXXFLAGS) -DMINE_ONLY -o $@ binary_trees.cpp $(TC_SRCS)

# Binary trees - Google
binary_trees_google: binary_trees.cpp
	$(CXX) $(CXXFLAGS) -DGOOGLE_ONLY $(GPERF_INCLUDE) -o $@ binary_trees.cpp $(GPERF_LIB)

# Binary trees - Sys
binary_trees_sys: binary_trees.cpp
	$(CXX) $(CXXFLAGS) -DSYS_ONLY -o $@ binary_trees.cpp

.PHONY: clean run
clean:
	rm -rf $(TARGETS)
run: all
	@echo "=== Mine (TCMemAllocator) ==="
	./malloc_bench_mine --benchmark_min_time=1 --benchmark_repetitions=1
	@echo ""
	@echo "=== Google (tcmalloc) ==="
	./malloc_bench_google --benchmark_min_time=1 --benchmark_repetitions=1
	@echo ""
	@echo "=== System malloc ==="
	./malloc_bench_sys --benchmark_min_time=1 --benchmark_repetitions=1
	@echo ""
	@echo "=== Binary Trees (depth=18, threads=4) ==="
	@echo "--- Mine ---"
	./binary_trees_mine 18 4
	@echo "--- Google ---"
	./binary_trees_google 18 4
	@echo "--- Sys ---"
	./binary_trees_sys 18 4
