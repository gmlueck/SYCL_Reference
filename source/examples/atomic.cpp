// SPDX-FileCopyrightText: 2020 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include <CL/sycl.hpp>

const int lsize = 10;

// Allows concurrent push or concurrent pop, but not both
struct List {
  // Points to free spot on list
  int *top;
  int *data;

#if __INTEL_CLANG_COMPILER == 20210300
  sycl::ONEAPI::atomic_ref<int *, sycl::ONEAPI::memory_order::relaxed,
                           sycl::ONEAPI::memory_scope::work_group,
                           sycl::access::address_space::global_space>
      atomic_top;
#elif __INTEL_CLANG_COMPILER >= 20210400
  sycl::ext::oneapi::atomic_ref<int *, sycl::ext::oneapi::memory_order::relaxed,
                                sycl::ext::oneapi::memory_scope::work_group,
                                sycl::access::address_space::global_space>
      atomic_top;
#else
  // Not tested
  sycl::atomic_ref<int *, sycl::memory_order::relaxed,
                   sycl::memory_scope::work_group>
      atomic_top;
#endif

  List(sycl::queue q, int size) : atomic_top(top) {
    data = sycl::malloc_shared<int>(size, q);
    top = data;
  };

  bool empty() { return top == data; };

  void push(int val) {
    // Allocate space by moving top pointer to right
    int *old_top = atomic_top.fetch_add(1);
    *old_top = val;
  };

  auto pop() {
    // Deallocate space by moving top pointer to left
    int *old_top = atomic_top.fetch_add(-1);
    return *(old_top - 1);
  };
};

int main() {
  sycl::queue q;

  auto in = new (sycl::malloc_shared<List>(1, q)) List(q, lsize);
  auto out = new (sycl::malloc_shared<List>(1, q)) List(q, lsize);

  q.parallel_for(lsize, [=](auto idx) { in->push(idx); }).wait();

  q.parallel_for(lsize, [=](auto idx) {
     out->push(idx * 100 + in->pop());
   }).wait();

  std::cout << "Out\n";
  while (!out->empty())
    std::cout << out->pop() << "\n";
}
