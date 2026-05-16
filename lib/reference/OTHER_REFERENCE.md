# Compiler Helpers Reference

## Quick Start

1. Include `lib/other.h` for all compiler compatibility macros and utility functions
2. Use `GCC_ATTR_*` macros instead of raw `__attribute__` — they degrade to no-ops on older/incompatible compilers
3. Use `likely()`/`unlikely()` for branch prediction hints in CAS loops and hot paths
4. Use `always_inline` for performance-critical inline functions
**5.** Add other work arounds and compatibility checks, maybe with a configure check

## Overview

`lib/other.h` is the compiler compatibility and abstraction layer. It provides:

- **GCC attribute wrappers** with version-gated fallbacks
- **Branch prediction hints** (`likely`/`unlikely`)
- **Inline control** (`always_inline`, `noinline`)
- **Cache prefetch hints**
- **Compiler barriers** (instruction and memory ordering)
- **CPU relax hints** for spin-loop backoff
- **Locale-safe character classification**
- **Platform constants** (pointer size, char bits)
- **Utility macros** (`container_of`, `anum`, stringification)

All macros degrade gracefully when the compiler does not support a given feature.

## Version Detection

| Macro | Purpose | Returns |
|-------|---------|---------|
| `_GNUC_PREREQ(maj, min)` | Test if GCC version >= (maj, min) | 1 if true, 0 if false or non-GCC |
| `_SUNC_PREREQ(ver)` | Test if SunPro C version >= ver | 1 if true, 0 if false or non-SunPro |
| `HAVE_GCC_ATTRIBUTE` | Defined if compiler supports `__attribute__` | Defined or undefined |

These macros are the gate for every other macro in this file. If the compiler version is too old, the macro expands to nothing.

**NOTE:** clang masks itself as GCC (other compiler like the SunPro also), so triggers the GCC check. Problem is the often not perfect compatibility (version mismatch (says its a gcc 4.3, but one feature of the 4.3 set is missing), compiler ICE, miscompiles, wrong/unfinshed/incompatible impl.). Thats why the Sun Compiler check even exists. clang would also need this. This is the file to abstract this support matrix nightmare away.

## GCC Attribute Macros

All `GCC_ATTR_*` macros expand to `__attribute__((...))` when the compiler supports the feature, or to nothing when it does not.

### Function Attributes

| Macro | GCC Min | Purpose | Usage Example |
|-------|---------|---------|---------------|
| `GCC_ATTR_COLD` | 4.3 | Hint: function is rarely called | `void __init cold_func(void)` |
| `GCC_ATTR_HOT` | 4.3 | Hint: function is frequently called | `void __hot hot_path(void)` |
| `GCC_ATTR_CONST` | 2.5 | Function is const (no other side effects, same input = same output) | `int hash(const void *p) GCC_ATTR_CONT;` |
| `GCC_ATTR_MALLOC` | 3.0 | Return value is malloc-like (no alias, aligned, usually non-NULL) | `void *my_alloc(size_t n);` |
| `GCC_ATTR_ALLOC_SIZE(n)` | 4.3 | Returned size is determined by parameter n | `void *my_alloc(size_t n) GCC_ATTR_ALLOC_SIZE(1);` |
| `GCC_ATTR_ALLOC_SIZE2(n, s)` | 4.3 | Returned size is n * s | `void *my_calloc(size_t n, size_t s) GCC_ATTR_ALLOC_SIZE2(1,2);` |
| `GCC_ATTR_OPTIMIZE(x)` | 4.4 | Set per-function optimization flags | `int GCC_ATTR_OPTIMIZE("O3") my_func(void *date) { ... }` |
| `GCC_ATTR_PURE` | 2.96 | Function is pure (only reads memory, no other side effects) | `int compute(const void *data) GCC_ATTR_PURE;` |
| `GCC_ATTR_PRINTF(str_ind, arg_ind)` | 2.3 | Printf-style format checking | `void my_log(const char *fmt, ...) GCC_ATTR_PRINTF(1,2);` |
| `GCC_ATTR_USED` | 3.1 | Prevent linker from discarding function | `void GCC_ATTR_USED used_fn(void)` |
| `GCC_ATTR_UNUSED_PARAM` | 3.1 | Suppress unused parameter warning | `void fn(int GCC_ATTR_UNUSED_PARAM ignored)` |
| `GCC_ATTR_CONSTRUCT` | 2.7 / SunPro 5.10 | Run before `main()` | Static constructor |
| `GCC_ATTR_DESTRUCT` | 2.7 / SunPro 5.10 | Run after `main()` exits | Static destructor |
| `GCC_ATTR_DLLEXPORT` | 2.8 (Win32/Cygwin) | Export from DLL | `__declspec(dllexport)` on Windows |
| `GCC_ATTR_FASTCALL` | 2.7 (x86 only) | Pass args in registers | x86 `__regparm__(3)` |
| `GCC_ATTR_SECTION(x)` | 2.6 | Place function in named section | `GCC_ATTR_SECTION(".hot")` |
| `GCC_ATTR_STDCALL` | 2.8 (Win32/Cygwin) | Windows stdcall convention | `__stdcall__` on Windows |
| `GCC_ATTR_VIS(x)` | 3.3 | Set symbol visibility | `GCC_ATTR_VIS("hidden")` |
| `GCC_ATTR_FALL_THROUGH` | 6.0 | Suppress implicit fallthrough warning in switch | After case label iff fallthrough is intended (e.g. state mashine) |
| `GCC_ATTR_TARGET(x)` | 11.1 | Force target-specific instructions | `GCC_ATTR_TARGET("avx2")` |
| `GCC_ATTR_ALIAS(x)` | 2.7 | Create function alias | `GCC_ATTR_ALIAS("real_fn")` |

### Variable Attributes

| Macro | GCC Min | Purpose | Usage Example | Note |
|-------|---------|---------|---------------|------|
| `GCC_ATTR_PACKED` | 2.7 | Remove structi/type padding | `struct s { char a; int b; } GCC_ATTR_PACKED` | bools are just one byte, but are often aligned to a native mashine word, this saves space, but can be very expensive used wrong |
| `GCC_ATTR_ALIGNED(n)` | 2.3 | Force alignment to n bytes | `GCC_ATTR_ALIGNED(64)` | |
| `GCC_ATTR_SECTION(x)` | 2.6 | Place variable in named section | `GCC_ATTR_SECTION(".initdata")` | |
| `GCC_ATTR_USED_VAR` | 3.3 | Prevent linker from discarding variable | `int GCC_ATTR_USED_VAR used_var` | |
| `GCC_ATTR_ASSUME_ALIGNED(n)` | 4.9 | Tell compiler pointer is aligned to n | On pointer return value | |
| `SECTION_GOT` | 2.6 | Place in `.got` section (when `GOT_GOT` defined) | GOT entry placement | do **NOT** use, for very very special purpose |

### Init Markers

| Macro | Expands To | Purpose |
|-------|------------|---------|
| `__init` | `GCC_ATTR_COLD` | Mark function as cold (run once at startup) |
| `__init_data` | nothing | Placeholder, for init-only data |
| `__init_cdata` | nothing | Placeholder, for init-only const data |
**TODO:** the data variants need implementation, full blown configure test if platform linkerscripts supports .initdata or equiv., or if GCC can do the job with ATTR_COLD on data, not functions

## Inline Control

| Macro | GCC Min | Purpose | Fallback |
|-------|---------|---------|----------|
| `always_inline` | 3.1 | Force inline (`inline __attribute__((__always_inline__))`) | `inline` |
| `noinline` | 3.1 | Prevent inlining (`__attribute__((__noinline__))`) | nothing |

## Branch Prediction

| Macro | GCC Min | Purpose | Fallback |
|-------|---------|---------|----------|
| `likely(x)` | 2.96 | `__builtin_expect(!!(x), 1)` | `(x)` |
| `unlikely(x)` | 2.96 | `__builtin_expect(!!(x), 0)` | `(x)` |

Use in CAS loop conditions and hot-path branches:
```c
} while (unlikely(expected != actual));
if (likely(ptr)) {
    /* fast path */
}
```

## Cache Prefetch

| Macro | GCC Min | Purpose | Fallback |
|-------|---------|---------|----------|
| `prefetch(x)` | 3.1 | Read-ahead hint (`__builtin_prefetch(x)`) | `(x)` |
| `prefetchw(x)` | 3.1 | Write-ahead hint (`__builtin_prefetch(x, 1)`) | `(x)` |
| `prefetch_nt(x)` | 3.1 | Non-temporal read hint (`__builtin_prefetch(x, 0, 0)`) | `(x)` |
| `prefetchw_nt(x)` | 3.1 | Non-temporal write hint (`__builtin_prefetch(x, 1, 0)`) | `(x)` |

Non-temporal variants skip the cache hierarchy — useful for streaming very large data.

## Overflow Detection

| Macro | GCC Min | Purpose | Fallback |
|-------|---------|---------|----------|
| `GCC_OVERFLOW_UMUL(a, b, res)` | 5.0 | `__builtin_umul_overflow(a, b, res)` | Manual divide-check: `a <= UINT_MAX / b` |
**NOTE** list of macros should be extended for other datatypes when need arises

Returns 1 if overflow would occur, stores result in `*res` (0 if overflow).

## Compiler Barriers

| Macro | GCC | Non-GCC | Purpose |
|-------|-----|---------|---------|
| `barrier()` | `asm volatile("")` | `do {} while (0)` | Compiler-only: prevents instruction reordering across the barrier |
| `mbarrier()` | `asm volatile("" ::: "memory")` | `do {} while (0)` | Compiler memory barrier: prevents load/store reordering |
| `mem_barrier(x)` | `asm volatile("": "=m"(*(x)))` | `do {} while (0)` | Mark memory location as accessed (for write-combining) |

See `lib/reference/ATOMIC_REFERENCE.md` for hardware memory barriers (`mb()`, `rmb()`, `wmb()`) which are architecture-specific and SMP-aware.

## CPU Relax

`cpu_relax()` inserts a processor-specific pause hint inside spin-loops to reduce contention and power consumption:

| Architecture | Instruction |
|-------------|-------------|
| x86/x86_64 | `pause` |
| PowerPC64 | `or 1,1,1; or 2,2,2` (HMT idle hint) |
| IA-64 | `hint @pause` |
| SPARC | `rd %%ccr, %%g0` |
| Other / `I_LIKE_ASM` disabled | `barrier()` |

Use `cpu_relax()` inside CAS retry loops to yield to other cores:
```c
while (atomic_cmpx(new, old, &lock) != old) {
    cpu_relax();
    /* retry */
}
```

## Locale-Safe Character Classification

These functions are locale-independent alternatives to `<ctype.h>` functions. They operate on `unsigned int` input and return 0 or 1.
-
| Function | Tests |
|----------|-------|
| `isspace_a(c)` | Space, tab (0x09-0x0D), and space character |
| `isblank_a(c)` | Space or tab only |
| `isdigit_a(c)` | '0'-'9' |
| `isalpha_a(c)` | 'a'-'z' or 'A'-'Z' |
| `isalnum_a(c)` | Letter or digit |
| `isgraph_a(c)` | Printable character (0x21-0x7E) |

`isblank(x)` is a C99 extention, but some systems are not C99 clean.
`isblank_a` is then also available as `isblank(x)` macro when `HAVE_ISBLANK` is not defined.
Some fallback is better then no compile because of this function.

## Platform Constants

| Macro | Purpose | Fallback Chain |
|-------|---------|----------------|
| `BITS_PER_CHAR` | Bits per char | `__CHAR_BIT__` → `CHAR_BIT` from `<limits.h>` |
| `BITS_PER_POINTER` | Bits per pointer (e.g. 32 or 64) | `__SIZEOF_POINTER__` → `SIZEOF_VOID_P` → `__WORDSIZE` → error |
| `SOTC` | `sizeof(uint16_t)` (always 2) | — |
| `BITS_PER_TCHAR` | Bits per `uint16_t` (always 16) | — |
**NOTE** TCHAR are windows ucs2/utf-16 `wchar_t`, protocol has this platform depency snuck in...

## Utility Macros

| Macro | Purpose | Example |
|-------|---------|---------|
| `container_of(ptr, type, member)` | Get struct from member pointer | Standard Linux kernel idiom |
| `anum(x)` | Array element count | `anum(arr)` → `sizeof(arr)/sizeof(arr[0])`, only works if sizeof `arr` is known! |
| `str_it(x)` | Stringify macro argument | `str_it(42)` → `"42"` |
| `DFUNC_NAME(fname, add)` | Concatenate function name parts | `DFUNC_NAME(my_calc, BITS_PER_CHAR)` → `my_calc8` |
| `DVAR_NAME(fname, add)` | Concatenate variable name parts | Same as `DFUNC_NAME` |

## Type Compatibility

| Macro/Type | Purpose |
|------------|---------|
| `DYN_ARRAY_LEN` | Flexible array member size: 0 for GCC/C99, 1 for non-GCC/non C99 |
| `restrict` | Defined as `restrict` keyword if C99, else empty |
| `sighandler_t` | Signal handler type: `sig_t` on FreeBSD, `void (*)(int)` otherwise |
| `UNALIGNED_OK` | Defined as 0 by default; set by configure if unaligned access is safe |
| `USE_SIMPLE_DISPATCH` | Defined if `mprotect` or `I_LIKE_ASM` unavailable |
| `HAVE_GCC_ATTRIBUTE` | Defined if `__attribute__` is supported |

## Linux TCP Options

On Linux, these define missing TCP socket options:

| Macro | Value | Purpose |
|-------|-------|---------|
| `TCP_THIN_LINEAR_TIMEOUTS` | 16 | Linear timeout for low-window connections |
| `TCP_THIN_DUPACK` | 17 | Fast recovery for thin streams |
| `TCP_USER_TIMEOUT` | 18 | Maximum time bytes may remain unacknowledged |

## Common Pitfalls

### 1. Assuming Attributes Exist

```c
// WRONG: __attribute__((aligned(64))) on non-GCC
struct data __attribute__((aligned(64)));

// CORRECT: macro degrades to nothing on unsupported compilers
struct data GCC_ATTR_ALIGNED(64);
```

### 2. Forgetting Constructor Requires Compiler Support

`GCC_ATTR_CONSTRUCT` requires GCC 2.7+ or SunPro 5.10+. The file `#error`s out on older compilers — this is intentional and non-optional.

### 3. Using `barrier()` Instead of Hardware Barriers

`barrier()` and `mbarrier()` are compiler-only fences. They do not order hardware memory operations. For SMP-correct memory ordering, use `mb()`, `rmb()`, or `wmb()` from the atomic library (see `lib/reference/ATOMIC_REFERENCE.md`).

### 4. `likely()`/`unlikely()` Are Hints Only

These macros affect branch prediction, not correctness. Code must work correctly regardless of prediction hints.

## References

- **Atomic operations:** `lib/reference/ATOMIC_REFERENCE.md` — uses `barrier()`, `mbarrier()`, `likely()`, `unlikely()`, `always_inline` from this file
- **Hazard pointers:** `lib/reference/HZP_REFERENCE.md` — lock-free memory reclamation
- **Implementation:** `lib/other.h`

---
*Last Updated: 2026-05-16*
