CXX = g++
CXXFLAGS = -std=c++17 -O2 -pthread -I../src -I../src/base
SRCDIR = ../src
TC_SRCS = $(SRCDIR)/ThreadCache.cpp $(SRCDIR)/CentralCache.cpp $(SRCDIR)/PageCache.cpp

TARGETS = 01_correctness 02_single_thread 03_scalability 04_stress \
          05_latency 06_fragmentation 07_endurance 08_peak_throughput 09_memory_capacity

all: $(TARGETS)

01_correctness: 01_correctness.cpp common.h $(TC_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ 01_correctness.cpp $(TC_SRCS)

02_single_thread: 02_single_thread.cpp common.h $(TC_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ 02_single_thread.cpp $(TC_SRCS)

03_scalability: 03_scalability.cpp common.h $(TC_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ 03_scalability.cpp $(TC_SRCS)

04_stress: 04_stress.cpp common.h $(TC_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ 04_stress.cpp $(TC_SRCS)

05_latency: 05_latency.cpp common.h $(TC_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ 05_latency.cpp $(TC_SRCS)

06_fragmentation: 06_fragmentation.cpp common.h $(TC_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ 06_fragmentation.cpp $(TC_SRCS)

07_endurance: 07_endurance.cpp common.h $(TC_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ 07_endurance.cpp $(TC_SRCS)

08_peak_throughput: 08_peak_throughput.cpp common.h $(TC_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ 08_peak_throughput.cpp $(TC_SRCS)

09_memory_capacity: 09_memory_capacity.cpp common.h $(TC_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ 09_memory_capacity.cpp $(TC_SRCS)

.PHONY: clean run
clean:
	rm -rf $(TARGETS) report_*.txt
run: all
	bash run_all.sh
