# Atomic Operations Reference Guide

## Quick Start

1. Include `lib/atomic.h` for all atomic types and operations
2. Use `atomic_t` for integer atomics, `atomicptr_t` for pointer atomics
3. Use `ATOMIC_INIT(x)` for static initialization
4. Use cross-platform API functions only (see Portable API section)
5. Pair with `hzp_ref()`/`hzp_unref()` for safe memory reclamation

## Overview

This document describes the **custom pre-C11 atomic operations library** used for lock-free data structures and lockless synchronization. The library provides architecture-specific inline assembly implementations with a GCC `__sync_*` builtin fallback and a lock-hash generic fallback. It is modeled after the Linux Kernel atomic primitives.

**TODO:** Refactor to `<stdatomic.h>` when C11 is available.

## Architecture

- **Tier 1 — Inline ASM:** Per-architecture inline assembly (x86, ARM, PPC, MIPS, Alpha, SPARC, IA-64)
- **Tier 2 — GCC Builtins:** `__sync_*` intrinsics (GCC 4.1+, all architectures)
- **Tier 3 — Lock-Hash Generic:** `lib/generic/atomic.c` uses a 4-entry lock table hashed by cache-line address

Selection is controlled by configure-time macros: `I_LIKE_ASM` enables inline ASM tier, `NEED_GENERIC` forces the lock-hash fallback.

## Types

| Type | Description | Internal Field | Array-Safe |
|------|-------------|----------------|------------|
| `atomic_t` | Atomic integer | `volatile int d` | No |
| `atomicptr_t` | Atomic pointer | `volatile void *d` | No |
| `atomicst_t` | Stack node (singly-linked next pointer) | `volatile atomicst_t *next` | No |
| `atomica_t` | Padded atomic integer for arrays | `volatile int d` or `char pad[ARCH_NEEDED_APAD]` | Yes |
| `atomicptra_t` | Padded atomic pointer for arrays | `volatile void *d` or `char pad[ARCH_NEEDED_APAD]` | Yes |
**NOTE:** Internal fields should not be accessed directly, internals may change, use accessors.

`ARCH_NEEDED_APAD` is 16 on Alpha and PowerPC (LL/SC architectures), 1 on others. This prevents LL/SC cache-line thrashing when atomics reside in arrays.
**TODO:** make shure all ll/SC architectures are covered, expand ifdefery.

### `atomicst_t` — Stack Nodes

The `atomicst_t` type represents a node in a lock-free stack. Its `next` pointer is the volatile field accessed by `atomic_sread()` and `atomic_cmpalx()`. The singly-linked structure is an implementation detail — conceptually it is a stack, not a list.

## Cross-Platform Portable API

These functions and macros are guaranteed to work across all supported architectures and all implementation tiers.

### Initialization

| Macro | Purpose |
|-------|---------|
| `ATOMIC_INIT(x)` | Static initializer: `atomic_t v = ATOMIC_INIT(0)` |
| `deatomic(x)` | Cast `atomicptr_t` value to `void *` via `intptr_t` to un-volatile the pointer |

### Integer Atomics (`atomic_t`)

| Function | Signature | Purpose | Return |
|----------|-----------|---------|--------|
| `atomic_read` | `int atomic_read(atomic_t *p)` | Load value | Current value |
| `atomic_set` | `void atomic_set(atomic_t *p, int v)` | Store value | — |
| `atomic_inc` | `void atomic_inc(atomic_t *p)` | Atomically increment by 1 | — |
| `atomic_inc_return` | `int atomic_inc_return(atomic_t *p)` | Atomically increment by 1 | **Old** value (before increment) |
| `atomic_inca_return` | `int atomic_inca_return(atomica_t *p)` | Same as `atomic_inc_return` for padded arrays | **Old** value (before increment) |
| `atomic_dec` | `void atomic_dec(atomic_t *p)` | Atomically decrement by 1 | — |
| `atomic_dec_test` | `int atomic_dec_test(atomic_t *p)` | Atomically decrement; test if zero | 1 if result is 0, else 0 |
| `atomic_x` | `int atomic_x(int new, atomic_t *p)` | Atomic exchange | **Old** value |
| `atomic_cmpx` | `int atomic_cmpx(int new, int old, atomic_t *p)` | Compare-and-swap | **Old** value (whether swap succeeded or not) |
| `atomic_bit_set` | `void atomic_bit_set(atomic_t *a, int bit)` | Atomically set bit | — |

### Pointer Atomics (`atomicptr_t`)

| Function | Signature | Purpose | Return |
|----------|-----------|---------|--------|
| `atomic_pread` | `void *atomic_pread(atomicptr_t *p)` | Load pointer | Current pointer |
| `atomic_pset` | `void atomic_pset(atomicptr_t *p, void *v)` | Store pointer | — |
| `atomic_px` | `void *atomic_px(void *new, atomicptr_t *p)` | Atomic pointer exchange | **Old** pointer |
| `atomic_cmppx` | `void *atomic_cmppx(void *new, void *old, atomicptr_t *p)` | Atomic pointer compare-and-swap | **Old** pointer (whether swap succeeded or not) |

### Padded Array Variants (`atomicptra_t` / `atomica_t`)

| Macro | Equivalent To | Notes |
|-------|---------------|-------|
| `atomic_pxa(val, ptr)` | `atomic_px(val, ptr)` | Casts pointer for padded array element access |
| `atomic_pxs(val, ptr)` | `atomic_px(val, ptr)` | Same as `atomic_pxa` (set variant) |
| `atomic_cmpalx(new, old, ptr)` | `atomic_cmppx(new, old, ptr)` | CAS for padded pointer arrays |
| `atomic_inca_return(ptr)` | `atomic_inc_return(ptr)` | Increment for padded integer arrays |

These macros cast a raw pointer to the appropriate padded atomic type. Use when atomics are elements of an array on LL/SC architectures to prevent cache-line thrashing.

### Stack Operations (`atomicst_t`)

| Function | Signature | Purpose | Return |
|----------|-----------|---------|--------|
| `atomic_push` | `void atomic_push(atomicst_t *head, atomicst_t *node)` | Push node onto lock-free stack | — |
| `atomic_pop` | `atomicst_t *atomic_pop(atomicst_t *head)` | Pop node from lock-free stack | Popped node, or NULL if empty |
| `atomic_sread` | `void *atomic_sread(atomicst_t *p)` | Read stack head/node next pointer | Current pointer |
| `atomic_sset` | `void atomic_sset(atomicst_t *p, atomicst_t *v)` | Set stack node next pointer | — |

**Critical:** `atomic_pop()` may return a non-deterministic element under contention. The lock-free pop uses a compare-and-swap loop that can "skip" nodes during concurrent modification. If you need strict FIFO/LIFO ordering, use a different data structure. For simple "give me something off the stack" usage, this is correct.

### Memory Barriers

| Macro | Purpose | SMP vs UP |
|-------|---------|-----------|
| `mb()` | Full memory barrier (orders loads and stores) | Hardware barrier on SMP, compiler-only on UP |
| `rmb()` | Read barrier (orders loads) | Hardware barrier on SMP, compiler-only on UP |
| `wmb()` | Write barrier (orders stores) | Hardware barrier on SMP, compiler-only on UP |
| `read_barrier_depends()` | Dependency ordering barrier | Architecture-dependent |

On UP (single-processor) builds, all barriers reduce to `mbarrier()` (compiler-only fence) except where architecture-specific ordering is required.

## Relationship with HZP

Atomic operations provide the **low-level primitives** for lock-free data structures. HZP (Hazard Pointers) provides **safe memory reclamation** for those structures. They are complementary:

- Use `atomic_px()` to atomically swap a shared pointer
- Use `hzp_ref()` to protect reading the shared pointer
- Use `hzp_unref()` to release the shared pointer
- Use `hzp_deferfree()` to defer freeing the old pointer
- `hzp_scan()` retires unreferenced objects, but is not ment to be called just-in-case

See `lib/reference/HZP_REFERENCE.md` for full HZP documentation.

## Arch-Specific Extensions (Implementation Artifacts)

The following functions are **NOT PORTABLE**. They exist only on specific architectures and should not be used in portable code. They are implementation artifacts/internal building blocks of the inline ASM tier.

**NOTE:** The portable API is deliberatly keept minimal. Nothing added "just in case".
It can be extended, say by `atomic_add`, if the need arises. But that means everything, all the implementations and fallbacks have to be edited/refactored.

### ARM
| Function | Notes |
|----------|-------|
| `atomic_add(int, atomic_t *)` | ARM-only |
| `atomic_sub(int, atomic_t *)` | ARM-only |
| `atomic_add_return(int, atomic_t *)` | ARM-only |
| `atomic_sub_return(int, atomic_t *)` | ARM-only |
| `atomic_set(atomic_t *, int)` | LL/SC based atomic set (not simple store), knowledge gap (Docs unclear) if all ARM versions and permutations are fine with store + fence |
| `atomic_pset(atomicptr_t *, void *)` | LL/SC based atomic set, see above |
| `atomic_sset(atomicst_t *, atomicst_t *)` | LL/SC based atomic set, see above |

### PowerPC
| Function | Notes |
|----------|-------|
| `atomic_dec_return(atomic_t *)` | Returns value after decrement |
| `atomic_inc_return(atomic_t *)` | Returns value before increment |
| `atomic_*_32()` / `atomic_*_64()` | Internal size-dispatch functions — do not call directly, gers cleaned up by compiler |

### MIPS
| Function | Notes |
|----------|-------|
| `atomic_add(int, atomic_t *)` | MIPS-only |
| `atomic_sub(int, atomic_t *)` | MIPS-only |
| `atomic_dec_return(atomic_t *)` | MIPS-only |

### Alpha
| Function | Notes |
|----------|-------|
| `atomic_add(int, atomic_t *)` | Alpha-only |
| `atomic_sub(int, atomic_t *)` | Alpha-only |
| `atomic_dec_return(atomic_t *)` | Alpha-only |

### SPARC
| Function | Notes |
|----------|-------|
| `atomic_add_return(int, atomic_t *)` | SPARC v9 only |
| `atomic_sub_return(int, atomic_t *)` | SPARC v9 only |

### IA-64
| Function | Notes |
|----------|-------|
| `atomic_fetch_and_add(int, atomic_t *)` | IA-64-only |
| `atomic_add_return(int, atomic_t *)` | IA-64-only (macro, uses fetchadd for powers of 2) |
| `atomic_sub_return(int, atomic_t *)` | IA-64-only (macro) |

## Implementation Details

### Generic Fallback (`lib/generic/atomic.c`)

When no inline ASM tier is available, and compiler can't provide `__sync_*` builtins, all operations use a **lock-hash table** with 4 `shortlock_t` locks. Lock selection hashes the pointer's cache-line address:

```c
#define ATOMIC_HASH_SIZE 4
#define ATOMIC_HASH(y) (&(gen_atomic_lock[(((intptr_t)(y))/L1_CACHE_BYTES) & (ATOMIC_HASH_SIZE-1)]))
```

Locks are initialized via a `GCC_ATTR_CONSTRUCT` static constructor. This tier has higher contention than inline ASM but is functionally correct on all platforms and is always available as a fail-safe.

### `atomic_push` Implementation

Defined in `lib/atomic.h` as a generic C fallback (guarded by `ATOMIC_PUSH_ARCH`). x86 overrides with inline ASM. Uses a CAS loop on the stack head pointer.

### `atomic_bit_set` Implementation

Defined in `lib/atomic.h` as a generic C fallback (guarded by `ATOMIC_BIT_SET_ARCH`). x86 overrides with `lock bts` instruction. The C fallback uses a CAS loop.

### Barrier and Compiler Helpers

Defined in `lib/other.h` — see `lib/reference/OTHER_REFERENCE.md` for full documentation.

| Macro | Purpose |
|-------|---------|
| `barrier()` | Compiler-only: prevents instruction reordering |
| `mbarrier()` | Compiler memory barrier: prevents load/store reordering |
| `likely()` | Branch prediction hint via `__builtin_expect` |
| `unlikely()` | Branch prediction hint via `__builtin_expect` |
| `always_inline` | Force inline via `__attribute__((always_inline))` |

## Common Pitfalls

### 1. Confusing Return Values

Both `atomic_cmpx` and `atomic_cmppx` return the **old value** of the target, regardless of whether the swap succeeded. To check success, compare the return value against the expected old value:

```c
// WRONG: assuming return value is the new value
int ret = atomic_cmpx(new_val, old_val, &atom);
// ret is old_val, not new_val!

// CORRECT: check if swap succeeded
if (atomic_cmpx(new_val, old_val, &atom) == old_val) {
    // swap succeeded
}
```

### 2. Using Arch-Specific Functions

```c
// WRONG: atomic_add is NOT portable
atomic_add(5, &counter);

// CORRECT: use portable API only
// (there is no portable add — use cmpxchg loop if needed, or extend API)
```

### 3. Forgetting Array Padding on LL/SC Architectures

```c
// WRONG on e.g. PPC/Alpha: adjacent elements thrash LL/SC watch
atomicptr_t arr[10];

// CORRECT: use padded type for arrays on LL/SC archs
atomicptra_t arr[10];
```

### 4. Misunderstanding `atomic_pop` Under Contention

```c
// atomic_pop may return any element from the top of the stack
// if concurrent push/pop operations are in progress.
// This is correct behavior, not a bug.
atomicst_t *node = atomic_pop(&stack_head);
// node may not be the exact top element if contention occurred
```

### 5. Missing Memory Barriers

Atomic operations do not guarantee memory ordering beyond the atomic variable itself. Use `mb()`, `rmb()`, or `wmb()` to order surrounding memory access:

```c
// Writer
data->value = 42;
mb();                            // Ensure store is visible before pointer swap
atomic_pset(&shared_ptr, data);

// Reader
void *p = atomic_pread(&shared_ptr);
if (p) {
    mb();                        // Ensure data load happens after pointer load
    printf("%d\n", ((struct data *)p)->value);
}
```

### 6. Static Initialization

```c
// WRONG
atomic_t counter = { 0 };

// CORRECT
atomic_t counter = ATOMIC_INIT(0);
```

## Performance Guidelines

### 1. Prefer Inline ASM Tier
Build with `I_LIKE_ASM` when targeting a supported architecture. Inline ASM avoids lock contention inherent in the generic fallback.

### 2. Minimize CAS Contention
High CAS retry rates degrade performance. Design lock-free structures to minimize conflicts on shared state. Use `cpu_relax()`

### 3. Use `atomicptra_t` / `atomica_t` for Arrays on LL/SC
On LL/SC architectes (e.g. Alpha and PowerPC), adjacent atomic elements in an array will thrash the LL/SC cache-line watch, maybe making complition of an atomic op nearly impossible at high contention (forgot which arch it was, but in trash case **NO** writer wins, **NO** forward progress). Padded types prevent this at the cost of larger memory footprint.

### 4. Generic Fallback Contention
The 4-entry lock table means up to 4 concurrent operations can proceed in parallel without blocking. High-contention workloads on the generic tier will bottleneck. Consider `I_LIKE_ASM` or GCC builtins.

### 5. Compiler Optimization
Use `likely()` and `unlikely()` in CAS loop conditions to help branch prediction:
```c
} while (unlikely(expected != actual));
```

## C11 `stdatomic.h` Migration Map

When migrating to C11, use this mapping as a reference:

| Current API | C11 Equivalent | Notes |
|-------------|----------------|-------|
| `atomic_t` | `_Atomic(int)` or `atomic_int` | Type changes |
| `atomicptr_t` | `_Atomic(void *)` or `atomic_ptrdiff_t` | Type changes |
| `ATOMIC_INIT(x)` | `ATOMIC_VAR_INIT(x)` | Macro rename |
| `atomic_read(p)` | `atomic_load_explicit(&p, memory_order_relaxed)` | Function call |
| `atomic_set(p, v)` | `atomic_store_explicit(&p, v, memory_order_relaxed)` | Function call |
| `atomic_inc(p)` | `atomic_fetch_add(&p, 1, memory_order_relaxed)` | Returns old value |
| `atomic_inc_return(p)` | `atomic_fetch_add(&p, 1, memory_order_seq_cst)` | Returns old value |
| `atomic_dec(p)` | `atomic_fetch_sub(&p, 1, memory_order_relaxed)` | Returns old value |
| `atomic_dec_test(p)` | `!atomic_fetch_sub(&p, 1, memory_order_seq_cst)` | Returns old value |
| `atomic_x(new, p)` | `atomic_exchange(&p, new, memory_order_seq_cst)` | Returns old value |
| `atomic_cmpx(new, old, p)` | `atomic_compare_exchange_weak(&p, &old, new)` | Returns bool, old updated |
| `atomic_px(new, p)` | `atomic_exchange(&p, new, memory_order_seq_cst)` | Returns old value |
| `atomic_cmppx(new, old, p)` | `atomic_compare_exchange_weak(&p, &old, new)` | Returns bool, old updated |
| `mb()` | `atomic_thread_fence(memory_order_seq_cst)` | Different semantics |
| `rmb()` | `atomic_thread_fence(memory_order_acquire)` | Different semantics |
| `wmb()` | `atomic_thread_fence(memory_order_release)` | Different semantics |

**Note:** C11 `atomic_compare_exchange_*` returns a boolean and updates the expected value, unlike `atomic_cmpx` which returns the old memory value directly. This is a behavioral difference that requires code changes.

## Validation Checklist

- [ ] Only cross-platform portable API functions are used
- [ ] No arch-specific extensions (`atomic_add`, `atomic_dec_return`, etc.) in shared code
- [ ] `atomicptra_t`/`atomica_t` used for atomic arrays on LL/SC architectures
- [ ] Memory barriers placed around atomic operations when ordering matters
- [ ] CAS return values are compared against expected old value to check success
- [ ] Static atomics initialized with `ATOMIC_INIT(x)`
- [ ] `atomic_pop()` return value is not assumed to be the exact top element under contention
- [ ] HZP is used alongside atomics for shared pointer reclamation

## Troubleshooting

| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| Deadlock on generic fallback | Too many threads contending on same 4-entry lock table | Build with `I_LIKE_ASM` for native CAS support |
| LL/SC thrashing on LL/SC arch | Atomic array elements share cache line | Use `atomicptra_t`/`atomica_t` padded types |
| CAS loop spins forever | Stale expected value, or concurrent writer | Verify expected value is re-read each loop iteration |
| Data corruption after atomic swap | Missing memory barrier | Add `mb()` or `wmb()` before/after atomic operation |
| ARM/MIPS inline ASM crashes | Unverified inline ASM constraints | Disable `I_LIKE_ASM` to fall back to GCC builtins or generic |
| `atomic_pop` returns unexpected element | Concurrent modification during pop | This is correct behavior — do not assume strict ordering |
| Wrong return value from `atomic_cmpx` | Expecting new value instead of old value | Compare return value against expected old value |

## References

- **Linux Kernel atomic.h:** Primary inspiration for API design and naming
- **Implementation:** `lib/atomic.h`, `lib/generic/atomic.c`, arch-specific `lib/*/atomic.h`
- **HZP Reference:** `lib/reference/HZP_REFERENCE.md` — safe memory reclamation companion
- **Barrier helpers:** `lib/other.h` — `barrier()`, `mbarrier()`, `likely()`, `unlikely()`
- **License:** GPL v3 (see `AGENTS.md` for full license details)

---
*Last Updated: 2026-05-16*
