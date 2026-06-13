// Ported from gperftools benchmark/malloc_bench.cc
// Three-way comparison: Mine (TCMemAllocator) / Google (tc_malloc) / Sys (malloc)
// Compile with -DMINE_ONLY, -DGOOGLE_ONLY, or -DSYS_ONLY

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <random>
#include <thread>

#include "run_benchmark.h"

// Prevent compiler from optimizing away allocation results
static inline void prevent_optimize(void* p) {
  asm volatile("" : : "r"(p) : "memory");
}

#if defined(MINE_ONLY)
#include "CMalloc.h"
using namespace CMP;
#define PREFIX "mine"
#define ALLOC(sz) TCMemAllocator::Alloc(sz)
#define FREE(p) TCMemAllocator::Free(p)
#define FREE_SIZED(p, sz) TCMemAllocator::Free(p, sz)
#elif defined(GOOGLE_ONLY)
#include "gperftools/tcmalloc.h"
#define PREFIX "google"
#define ALLOC(sz) tc_malloc(sz)
#define FREE(p) tc_free(p)
#define FREE_SIZED(p, sz) tc_free(p)
#elif defined(SYS_ONLY)
#define PREFIX "sys"
#define ALLOC(sz) malloc(sz)
#define FREE(p) free(p)
#define FREE_SIZED(p, sz) free(p)
#else
#error "Define one of MINE_ONLY, GOOGLE_ONLY, SYS_ONLY"
#endif

// ==================== Benchmark functions ====================

static void bench_fastpath_throughput(long iterations, uintptr_t param) {
  size_t sz = 32;
  for (; iterations > 0; iterations--) {
    void* p = ALLOC(sz);
    prevent_optimize(p);
    FREE(p);
    sz = ((sz * 8191) & 511) + 16;
  }
}

static void bench_fastpath_dependent(long iterations, uintptr_t param) {
  size_t sz = 32;
  for (; iterations > 0; iterations--) {
    uintptr_t p = reinterpret_cast<uintptr_t>(ALLOC(sz));
    prevent_optimize(reinterpret_cast<void*>(p));
    FREE(reinterpret_cast<void*>(p));
    sz = ((sz | static_cast<size_t>(p)) & 511) + 16;
  }
}

static void bench_fastpath_simple(long iterations, uintptr_t param) {
  size_t sz = static_cast<size_t>(param);
  for (; iterations > 0; iterations--) {
    void* p = ALLOC(sz);
    prevent_optimize(p);
    FREE(p);
  }
}

static void bench_fastpath_simple_sized(long iterations, uintptr_t param) {
  size_t sz = static_cast<size_t>(param);
  for (; iterations > 0; iterations--) {
    void* p = ALLOC(sz);
    prevent_optimize(p);
    FREE_SIZED(p, sz);
  }
}

static void bench_fastpath_stack(long iterations, uintptr_t _param) {
  size_t sz = 64;
  long param = static_cast<long>(_param);
  param = std::max(1l, param);
  std::unique_ptr<void*[]> stack = std::make_unique<void*[]>(param);
  for (; iterations > 0; iterations -= param) {
    for (long k = param - 1; k >= 0; k--) {
      void* p = ALLOC(sz);
      stack[k] = p;
      sz = ((sz | reinterpret_cast<size_t>(p)) & 511) + 16;
    }
    for (long k = 0; k < param; k++) {
      FREE(stack[k]);
    }
  }
}

static void bench_fastpath_stack_simple(long iterations, uintptr_t _param) {
  size_t sz = 32;
  long param = static_cast<long>(_param);
  param = std::max(1l, param);
  std::unique_ptr<void*[]> stack = std::make_unique<void*[]>(param);
  for (; iterations > 0; iterations -= param) {
    for (long k = param - 1; k >= 0; k--) {
      void* p = ALLOC(sz);
      stack[k] = p;
    }
    for (long k = 0; k < param; k++) {
      FREE_SIZED(stack[k], sz);
    }
  }
}

static void bench_fastpath_rnd_dependent(long iterations, uintptr_t _param) {
  static const uintptr_t rnd_c = 1013904223;
  static const uintptr_t rnd_a = 1664525;

  size_t sz = 128;
  if ((_param & (_param - 1))) {
    abort();
  }

  long param = static_cast<long>(_param);
  param = std::max(1l, param);
  std::unique_ptr<void*[]> ptrs = std::make_unique<void*[]>(param);

  for (; iterations > 0; iterations -= param) {
    for (int k = param - 1; k >= 0; k--) {
      void* p = ALLOC(sz);
      ptrs[k] = p;
      sz = ((sz | reinterpret_cast<size_t>(p)) & 511) + 16;
    }

    uint32_t rnd = 0;
    uint32_t free_idx = 0;
    do {
      FREE(ptrs[free_idx]);
      rnd = rnd * rnd_a + rnd_c;
      free_idx = rnd & (param - 1);
    } while (free_idx != 0);
  }
}

static void bench_fastpath_rnd_dependent_8cores(long iterations, uintptr_t _param) {
  static const uintptr_t rnd_c = 1013904223;
  static const uintptr_t rnd_a = 1664525;

  if ((_param & (_param - 1))) {
    abort();
  }

  long param = static_cast<long>(_param);
  param = std::max(1l, param);

  auto body = [iterations, param]() {
    size_t sz = 128;
    std::unique_ptr<void*[]> ptrs = std::make_unique<void*[]>(param);

    for (long i = iterations; i > 0; i -= param) {
      for (int k = param - 1; k >= 0; k--) {
        void* p = ALLOC(sz);
        ptrs[k] = p;
        sz = ((sz | reinterpret_cast<size_t>(p)) & 511) + 16;
      }

      uint32_t rnd = 0;
      uint32_t free_idx = 0;
      do {
        FREE(ptrs[free_idx]);
        rnd = rnd * rnd_a + rnd_c;
        free_idx = rnd & (param - 1);
      } while (free_idx != 0);
    }
  };

  std::thread ts[] = {std::thread{body}, std::thread{body}, std::thread{body}, std::thread{body},
                      std::thread{body}, std::thread{body}, std::thread{body}, std::thread{body}};
  for (auto& t : ts) {
    t.join();
  }
}

// ==================== Randomize freelists ====================

#if defined(MINE_ONLY)
void randomize_one_size_class(size_t size) {
  size_t count = (10 << 20) / size;  // 10MB per size class
  auto randomize_buffer = std::make_unique<void*[]>(count);

  for (size_t i = 0; i < count; i++) {
    randomize_buffer[i] = TCMemAllocator::Alloc(size);
  }

  std::shuffle(randomize_buffer.get(), randomize_buffer.get() + count, std::minstd_rand(rand()));

  for (size_t i = 0; i < count; i++) {
    TCMemAllocator::Free(randomize_buffer[i]);
  }
}

void randomize_size_classes() {
  randomize_one_size_class(8);
  int i;
  for (i = 16; i < 256; i += 16) {
    randomize_one_size_class(i);
  }
  for (; i < 512; i += 32) {
    randomize_one_size_class(i);
  }
  for (; i < 1024; i += 64) {
    randomize_one_size_class(i);
  }
  for (; i < (4 << 10); i += 128) {
    randomize_one_size_class(i);
  }
  for (; i < (32 << 10); i += 1024) {
    randomize_one_size_class(i);
  }
}
#endif

// ==================== Main ====================

int main(int argc, char** argv) {
  init_benchmark(&argc, &argv);

#if defined(MINE_ONLY)
  if (!benchmark_list_only) {
    printf("Trying to randomize freelists...");
    fflush(stdout);
    randomize_size_classes();
    printf("done.\n");
  }
#endif

  report_benchmark(PREFIX "_fastpath_throughput", bench_fastpath_throughput, 0);
  report_benchmark(PREFIX "_fastpath_dependent", bench_fastpath_dependent, 0);
  report_benchmark(PREFIX "_fastpath_simple", bench_fastpath_simple, 64);
  report_benchmark(PREFIX "_fastpath_simple", bench_fastpath_simple, 2048);
  report_benchmark(PREFIX "_fastpath_simple", bench_fastpath_simple, 16384);
  report_benchmark(PREFIX "_fastpath_simple_sized", bench_fastpath_simple_sized, 64);
  report_benchmark(PREFIX "_fastpath_simple_sized", bench_fastpath_simple_sized, 2048);

  for (int i = 8; i <= 512; i <<= 1) {
    report_benchmark(PREFIX "_fastpath_stack", bench_fastpath_stack, i);
  }

  report_benchmark(PREFIX "_fastpath_stack_simple", bench_fastpath_stack_simple, 32);
  report_benchmark(PREFIX "_fastpath_stack_simple", bench_fastpath_stack_simple, 8192);
  report_benchmark(PREFIX "_fastpath_stack_simple", bench_fastpath_stack_simple, 32768);

  report_benchmark(PREFIX "_fastpath_rnd_dependent", bench_fastpath_rnd_dependent, 32);
  report_benchmark(PREFIX "_fastpath_rnd_dependent", bench_fastpath_rnd_dependent, 8192);
  report_benchmark(PREFIX "_fastpath_rnd_dependent", bench_fastpath_rnd_dependent, 32768);

  report_benchmark(PREFIX "_fastpath_rnd_dependent_8cores", bench_fastpath_rnd_dependent_8cores, 32768);

  return 0;
}
