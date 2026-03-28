///////////////////
// NOTE: Intrisics
#ifndef RSTD_INTRINSICS_H
#define RSTD_INTRINSICS_H

#if COMPILER_CLANG || COMPILER_GCC
  #define force_inline inline __attribute__((always_inline))
#elif COMPILER_MSVC
  #define force_inline __forceinline
#endif

#if COMPILER_MSVC || (COMPILER_CLANG && OS_WINDOWS)
  #pragma section(".rdata$", read)
  #define read_only __declspec(allocate(".rdata$"))
#elif COMPILER_CLANG
  #define read_only __attribute__((section(".rodata")))
#elif COMPILER_GCC
  /* TODO(rnp): not supported on GCC, putting it in rodata causes warnings and writing to
   * it doesn't cause a fault */
  #define read_only
#endif

#if COMPILER_MSVC

  #define alignas(n)     __declspec(align(n))
  #define no_return      __declspec(noreturn)

  #define likely(x)      (x)
  #define unlikely(x)    (x)

  #define assume(x)      __assume(x)
  #define debugbreak     __debugbreak
  #define unreachable()  __assume(0)

  #if ARCH_ARM64
    #define cpu_yield()  __yield()
  #endif

#else /* !COMPILER_MSVC */

  #define alignas(n)     __attribute__((aligned(n)))
  #define no_return      __attribute__((noreturn))

  #define likely(x)      (__builtin_expect(!!(x), 1))
  #define unlikely(x)    (__builtin_expect(!!(x), 0))

  #if COMPILER_CLANG
    #define assume(x)    __builtin_assume(x)
  #else
    #define assume(x)    __attribute__((assume(x)))
  #endif
  #define unreachable()  __builtin_unreachable()

  #if ARCH_ARM64
    /* TODO(rnp)? debuggers just loop here forever and need a manual PC increment (step over) */
    #define debugbreak() asm volatile ("brk 0xf000")
    #define cpu_yield()  asm volatile ("yield")
  #else
    #define debugbreak() asm volatile ("int3; nop")
  #endif

#endif /* !COMPILER_MSVC */

#if ARCH_X64
  #define cpu_yield()    _mm_pause()
#endif

#endif /* RSTD_INTRINSICS_H */
