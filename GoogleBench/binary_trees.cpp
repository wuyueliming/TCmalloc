// Ported from gperftools benchmark/binary_trees.cc
// Three-way comparison: Mine (TCMemAllocator) / Google (tc_malloc) / Sys (malloc)
// Compile with -DMINE_ONLY, -DGOOGLE_ONLY, or -DSYS_ONLY

#include <algorithm>
#include <errno.h>
#include <iostream>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <chrono>

#if defined(MINE_ONLY)
#include "CMalloc.h"
using namespace CMP;
#define ALLOCATOR_NAME "Mine (TCMemAllocator)"
struct Node {
  Node *l, *r;
  int i;
  static void* operator new(size_t size) {
    return TCMemAllocator::Alloc(size);
  }
  static void operator delete(void* p) {
    TCMemAllocator::Free(p);
  }
  Node(int i2) : l(0), r(0), i(i2) {}
  Node(Node* l2, int i2, Node* r2) : l(l2), r(r2), i(i2) {}
  ~Node() { delete l; delete r; }
  int check() const { return l ? l->check() + i - r->check() : i; }
};
#elif defined(GOOGLE_ONLY)
#include "gperftools/tcmalloc.h"
#define ALLOCATOR_NAME "Google (tcmalloc)"
struct Node {
  Node *l, *r;
  int i;
  static void* operator new(size_t size) {
    return tc_malloc(size);
  }
  static void operator delete(void* p) {
    tc_free(p);
  }
  Node(int i2) : l(0), r(0), i(i2) {}
  Node(Node* l2, int i2, Node* r2) : l(l2), r(r2), i(i2) {}
  ~Node() { delete l; delete r; }
  int check() const { return l ? l->check() + i - r->check() : i; }
};
#elif defined(SYS_ONLY)
#define ALLOCATOR_NAME "System (malloc)"
struct Node {
  Node *l, *r;
  int i;
  Node(int i2) : l(0), r(0), i(i2) {}
  Node(Node* l2, int i2, Node* r2) : l(l2), r(r2), i(i2) {}
  ~Node() { delete l; delete r; }
  int check() const { return l ? l->check() + i - r->check() : i; }
};
#else
#error "Define one of MINE_ONLY, GOOGLE_ONLY, SYS_ONLY"
#endif

Node* make(int i, int d) {
  if (d == 0) return new Node(i);
  return new Node(make(2 * i - 1, d - 1), i, make(2 * i, d - 1));
}

void run(int given_depth) {
  int min_depth = 4, max_depth = std::max(min_depth + 2, given_depth), stretch_depth = max_depth + 1;

  {
    Node* c = make(0, stretch_depth);
    std::cout << "[" ALLOCATOR_NAME "] stretch tree of depth " << stretch_depth
              << "\t check: " << c->check() << std::endl;
    delete c;
  }

  Node* long_lived_tree = make(0, max_depth);

  for (int d = min_depth; d <= max_depth; d += 2) {
    int iterations = 1 << (max_depth - d + min_depth), c = 0;
    for (int i = 1; i <= iterations; ++i) {
      Node *a = make(i, d), *b = make(-i, d);
      c += a->check() + b->check();
      delete a;
      delete b;
    }
    std::cout << "[" ALLOCATOR_NAME "] " << (2 * iterations)
              << "\t trees of depth " << d << "\t check: " << c << std::endl;
  }

  std::cout << "[" ALLOCATOR_NAME "] long lived tree of depth " << max_depth
            << "\t check: " << (long_lived_tree->check()) << "\n";

  delete long_lived_tree;
}

static void* run_tramp(void* _a) {
  intptr_t a = reinterpret_cast<intptr_t>(_a);
  run(a);
  return 0;
}

int main(int argc, char* argv[]) {
  int given_depth = argc >= 2 ? atoi(argv[1]) : 18;
  int thread_count = std::max(1, argc >= 3 ? atoi(argv[2]) : 1);

  printf("=== Binary Trees Benchmark ===\n");
  printf("Allocator: %s\n", ALLOCATOR_NAME);
  printf("Depth: %d, Threads: %d\n\n", given_depth, thread_count);

  auto start = std::chrono::high_resolution_clock::now();

  std::vector<pthread_t> threads(thread_count - 1);
  for (int i = 0; i < thread_count - 1; i++) {
    int rv = pthread_create(&threads[i], nullptr, run_tramp, reinterpret_cast<void*>(given_depth));
    if (rv) { errno = rv; perror("pthread_create"); }
  }
  run(given_depth);
  for (int i = 0; i < thread_count - 1; i++) {
    pthread_join(threads[i], nullptr);
  }

  auto end = std::chrono::high_resolution_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  printf("\n[%s] Total time: %.2f ms\n", ALLOCATOR_NAME, ms);

  return 0;
}
