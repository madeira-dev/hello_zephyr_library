#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <new>
#include <cstdlib>

// =========================================================================
// STRATEGY: Static Kernel Heap (k_heap)
// =========================================================================
// We reserve 132KB of RAM specifically for C++ objects (OpenFHE).
// This sits in the BSS section, independent of the system malloc heap.
// Calculation: 256KB Total - 32KB (System/Stack) - 132KB (This Heap) = ~92KB Free
// =========================================================================
K_HEAP_DEFINE(cpp_heap, 132 * 1024);

// Optional: Debug counter to verify our heap is actually being used
static size_t allocation_counter = 0;

void *operator new(size_t size)
{
  // Try to allocate from our static k_heap
  void *ptr = k_heap_alloc(&cpp_heap, size, K_NO_WAIT);

  if (ptr)
  {
    allocation_counter++;
    // Uncomment for verbose debugging (spammy!)
    // printk("[new] Alloc %zu bytes -> %p (Count: %zu)\n", size, ptr, allocation_counter);
    return ptr;
  }
  else
  {
    printk("!!! [new] FAILED to allocate %zu bytes. Heap Full! !!!\n", size);
#if defined(__cpp_exceptions)
    throw std::bad_alloc();
#else
    return nullptr;
#endif
  }
}

void operator delete(void *ptr) noexcept
{
  if (ptr)
  {
    k_heap_free(&cpp_heap, ptr);
    // printk("[delete] Freed %p\n", ptr);
  }
}

// =========================================================================
// Array Overrides (std::vector uses these often)
// =========================================================================
void *operator new[](size_t size)
{
  return operator new(size);
}

void operator delete[](void *ptr) noexcept
{
  operator delete(ptr);
}

// =========================================================================
// Sized Deallocations (Required for C++14/17 compliance)
// =========================================================================
void operator delete(void *ptr, size_t size) noexcept
{
  operator delete(ptr);
}

void operator delete[](void *ptr, size_t size) noexcept
{
  operator delete(ptr);
}
