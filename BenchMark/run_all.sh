#!/bin/bash
# TC_malloc Benchmark Suite - Run All Tests
# 每个测试独立进程运行，测试前后清理系统资源

REPORT=report_$(date +%Y%m%d_%H%M%S).txt

drop_caches() {
    sync
    echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null 2>&1
    sleep 1
}

echo "================================================================" | tee $REPORT
echo "  TC_malloc Benchmark Suite" | tee -a $REPORT
echo "  Machine: $(nproc) threads, $(free -h | awk '/Mem:/{print $2}') RAM" | tee -a $REPORT
echo "  Date: $(date)" | tee -a $REPORT
echo "================================================================" | tee -a $REPORT
echo "" | tee -a $REPORT

TESTS="01_correctness 02_single_thread 03_scalability 04_stress \
       05_latency 06_fragmentation 07_endurance 08_peak_throughput 09_memory_capacity"

FAILED=0
PASSED=0

for test in $TESTS; do
    if [ ! -x "$test" ]; then
        echo ">>> SKIP $test (not found or not executable)" | tee -a $REPORT
        continue
    fi

    echo ">>> Running $test..." | tee -a $REPORT
    echo "---" | tee -a $REPORT
    drop_caches
    ./$test 2>&1 | tee -a $REPORT
    exit_code=$?
    echo "---" | tee -a $REPORT

    if [ $exit_code -eq 0 ]; then
        echo ">>> $test: PASSED" | tee -a $REPORT
        PASSED=$((PASSED + 1))
    else
        echo ">>> $test: FAILED (exit code: $exit_code)" | tee -a $REPORT
        FAILED=$((FAILED + 1))
    fi

    echo "" | tee -a $REPORT
    drop_caches
    sleep 2
done

echo "================================================================" | tee -a $REPORT
echo "  All tests completed. PASSED: $PASSED, FAILED: $FAILED" | tee -a $REPORT
echo "  Report saved to: $REPORT" | tee -a $REPORT
echo "================================================================" | tee -a $REPORT

# ==================== Google Benchmarks ====================
echo "" | tee -a $REPORT
echo "================================================================" | tee -a $REPORT
echo "  Google Benchmarks (ported from gperftools)" | tee -a $REPORT
echo "================================================================" | tee -a $REPORT
echo "" | tee -a $REPORT

GOOGLE_BENCH_DIR="../GoogleBench"

if [ -d "$GOOGLE_BENCH_DIR" ]; then
    # Build Google benchmarks
    echo ">>> Building Google benchmarks..." | tee -a $REPORT
    (cd "$GOOGLE_BENCH_DIR" && make clean && make) 2>&1 | tee -a $REPORT
    if [ $? -ne 0 ]; then
        echo ">>> Google benchmarks build FAILED" | tee -a $REPORT
    else
        echo ">>> Google benchmarks build OK" | tee -a $REPORT
        echo "" | tee -a $REPORT

        # Run malloc_bench
        if [ -x "$GOOGLE_BENCH_DIR/malloc_bench" ]; then
            echo ">>> Running malloc_bench..." | tee -a $REPORT
            echo "---" | tee -a $REPORT
            drop_caches
            $GOOGLE_BENCH_DIR/malloc_bench 2>&1 | tee -a $REPORT
            echo "---" | tee -a $REPORT
            echo "" | tee -a $REPORT
        fi

        # Run binary_trees
        if [ -x "$GOOGLE_BENCH_DIR/binary_trees" ]; then
            echo ">>> Running binary_trees (depth=18, threads=4)..." | tee -a $REPORT
            echo "---" | tee -a $REPORT
            drop_caches
            $GOOGLE_BENCH_DIR/binary_trees 18 4 2>&1 | tee -a $REPORT
            echo "---" | tee -a $REPORT
            echo "" | tee -a $REPORT
        fi
    fi
else
    echo ">>> SKIP Google benchmarks (directory not found: $GOOGLE_BENCH_DIR)" | tee -a $REPORT
fi
