# Hazard Pointers (HZP) Reference Guide

## Quick Start

1. Add your subsystem enum to `lib/hzp.h` before `HZP_MAX`
2. Call `hzp_alloc()` at thread creation
3. Use `hzp_ref()` / `hzp_unref()` around all shared data access
4. Use `hzp_deferfree()` when replacing shared pointers
5. Call `hzp_scan()` on main thread periodically

## Overview
This document describes the **HZP (Hazard Pointers)** implementation used for safe memory reclamation in lock-free data structures. The system prevents use-after-free errors by tracking active references to shared memory across threads.

## Architecture
- **TLS-based:** Each thread maintains its own local hazard pointer storage (`local_hzp`).
- **Subsystems:** Multiple subsystems can track references simultaneously via `enum hzps` keys.
- **Deferred Freeing:** Objects are not freed immediately when the last writer releases them. They are queued and freed only when no reader holds a reference.
- **Scanning:** Periodic `hzp_scan()` calls retire and free objects via callback that are no longer referenced.

## Core Concepts

### 1. Reference Acquisition (Reader)
When a thread wants to read shared data that might be freed, it must:
1. Read the pointer into a **local variable** (volatile).
2. Register it with `hzp_ref()`.
3. Re-read the pointer to ensure it hasn't changed mid-read.
4. Repeat until stable.

```c
struct shared_data * volatile data;
struct shared_data *local_ref;

do {
    local_ref = (void *)data; // Snapshot the pointer. Cast to void* suppresses volatile warnings but preserves the address.
    hzp_ref(HZP_mysubsys, local_ref);  // Register as hazard pointer
    // Use local_ref safely now
} while (local_ref != data);           // Retry if data changed
```

**Critical:** The `data` variable must be `volatile` or other means (e.g. compiler barriers) need to be used to prevent compiler optimization from reordering or removing reads. Weak memory ordering architectures might need additional memory barriers.

### 2. Reference Release (Reader)
After reading is done and before the thread exits the critical section:
```c
hzp_unref(HZP_mysubsys);  // Unregister hazard pointer
```

**Critical:** Always call `hzp_unref()` **after** the last access. Forgetting this causes memory leaks.

### 3. Deferred Freeing (Writer)
When a writer replaces shared data:
```c
// real implementation needs memory barriers or atomic exchange
struct shared_data *old_data = data;
data = new_data;          // Install new pointer

// Defer freeing the old object
struct hzp_free *to_free = my_allocator_alloc();
to_free->data = old_data;
to_free->free_func = shared_data_free;
hzp_deferfree(to_free, old_data, shared_data_free); // shared_data_free() called with old_data as argument
```

**Critical:** The old data cannot be freed immediately, even if no writer holds it. Readers might still hold a reference.

### 4. Scanning and Retiring
The main thread (or a dedicated thread) should call `hzp_scan()` periodically:
```c
// NORM_HZP_THRESHOLD is a numeric constant defined in hzp.h to give a reasenable lower threashold
hzp_scan(NORM_HZP_THRESHOLD);  // Retire objects when amount exceeds threshold
```

**Critical:** Call `hzp_scan()` frequently enough to avoid memory buildup, but not so frequently that it blocks performance.

## Thread Lifecycle

### Thread Start
```c
bool ok = hzp_alloc();  // Allocate TLS storage. Must be called once at thread creation.
if (!ok) {
    // Handle error
}
```

### Thread Exit
```c
hzp_free();  // Mark TLS as unused, allow reclamation
```

**Critical:** Call `hzp_free()` before thread termination to prevent dangling references.

## API Reference

| Function | Purpose | Thread-Safe | Notes |
|----------|---------|-------------|-------|
| `hzp_alloc()` | Allocate TLS storage for thread | N/A | Call once per thread start |
| `hzp_free()` | Deallocate TLS storage | N/A | Call once per thread exit |
| `hzp_ref(key, ptr)` | Register hazard pointer | Yes | Use after reading shared data |
| `hzp_unref(key)` | Unregister hazard pointer | Yes | Use after last access |
| `hzp_deferfree(item, data, free_func)` | Queue object for deferred free | Yes | Called by writers |
| `hzp_scan(threshold)` | Scan and free unreferenced objects | Main thread only | executes all free callbacks |

## Common Pitfalls

### 1. Missing `hzp_unref()`
```c
// WRONG: Memory leak
hzp_ref(HZP_subsys, ptr);
// ... use ptr ...
// Forgot to call hzp_unref()
```

### 2. Premature Unregistration (`hzp_unref`)
- **Problem:** Calling `hzp_unref()` before you have **finished** using the shared data.
- **Consequence:** `hzp_scan()` sees no active references, correctly frees the memory, and your thread subsequently dereferences a freed pointer (Use-After-Free).
- **Solution:** Keep `hzp_ref()` active for the **entire duration** of the critical section. Only call `hzp_unref()` immediately after the last read/write operation.
- **Pattern:**
  ```c
  do {
      local_ref = shared_data;
      hzp_ref(HZP_subsys, local_ref);
      // ... use local_ref ...
  } while (local_ref != shared_data);
  hzp_unref(HZP_subsys); // ONLY AFTER the loop finishes
  ```

### 3. Compiler Reordering
```c
// WRONG: Compiler may reorder reads
shared_data = ptr;
hzp_ref(HZP_subsys, shared_data);
```

**Fix:** Use `volatile` and memory barriers:
```c
shared_data = (my_data_t *volatile) ptr;
barrier(); // Prevents compiler reordering of this read with subsequent hazard pointer registration. Harder order requirements are needed for weakly ordered archs
hzp_ref(HZP_subsys, shared_data);
```

### 4. Unaligned Access
```c
// WRONG: May trigger false sharing/double cache line fetch
struct my_struct *ptr = (void *)ptr;  // Not 64-byte aligned
```

**Fix:** Use `GCC_ATTR_ALIGNED(64)` on structures used as hazard pointers.

### 5. Calling `hzp_scan()` Too Rarely
**Problem:** Deferred free list grows, consuming memory.

**Solution:** Call `hzp_scan()` on every N events or at fixed intervals.

### 6. **`hzp_scan` Stack Usage
- **Problem:** `hzp_scan()` deliberately uses `alloca()` to collect active hazard pointers while avoiding system memory allocations.
- **Scaling:** `hzp_scan()` uses stack memory proportional to active threads, number of subsystems, and occupancy (fractional value < 1). Scaling is bound by sane outer limits, not in the code itself. This is designed for core-count threads, limited fixed number of subsystems, not massive-scale deployments.
- **Consequence:** With many hzp participating threads and hzp subsystems and references hold simultaneously, this can exceed the default stack size. This can lead to a Crash (SIGSEGV) when `hzp_scan()` is called.
- **Solution:** Ensure the thread calling `hzp_scan()` has a large stack.
- **Note:** Do **not** run `hzp_scan()` inside worker threads with small stacks. Be aware some OS (notably BSD) have small non-main thread default stack sizes. Run it on the main thread (usally has large stack by default) or a dedicated collector thread with a fat stack (e.g. `pthread_attr_setstacksize`).

## Performance Guidelines

### 1. Minimize `hzp_scan()` Calls
- Do not scan in hot paths.
- Do not scan too often, this is an expensive operation (CPU inter core cache invalidation traffic)

### 2. Minimize writes
- hzp is meant for low/seldom writer, many reader lockless paradigms
- it is additionally very benefical when "reader worked with stale data" is not an catastrophic error (simply use result, no retry, no rollback) can be exploited

### 3. Use TLS Where Possible
- Avoid shared hazard pointer storage if possible. It helps, but is no magic bullet.
- TLS reduces contention.

## Configuration

### Subsystems
Define subsystem keys in `lib/hzp.h`:
```c
enum hzps {
    // ... exixting subsystem entries
    HZP_mysubsys,
    HZP_another_subsys,
    /* keep this the last entry!! */
    HZP_MAX  // determines array size per thread
};
```

### Stack Requirements
- Main thread: Default OS stack size (typically 8MB+)
- Worker threads: Do **not** call `hzp_scan()` on small stacks
- Collector threads: Use `pthread_attr_setstacksize()` if needed

## Examples
### Safe Reader Pattern
```c
bool safe_read(struct shared_data ** volatile ptr)
{
    struct shared_data *data;
    bool ret = false;

    do {
        // cast to void * to prevent compiler warning about un-volatiling data
        // rare case where we **actually** want to do that
        data = (void *)*ptr;
        barrier();
        if (data)
            hzp_ref(HZP_thesubsystem, data);
        else
            return false;
    } while (data != *ptr);

    // Use data safely, even for bigger amounts of instructions
    // set return value appropriately

    barrier();
    hzp_unref(HZP_thesubsystem);
    // if it is important the data is really fresh/up to date, recheck
    // input pointer against local pointer and if not equal loop to top

    return ret;
}
```

### Safe Writer Pattern
```c
bool safe_write(struct shared_data **ptr, struct shared_data *new_data)
{
    struct shared_data *old_data;

    // use our pre C11 kernel inspired functions and types: atomic pointer exchange
    old_data = atomic_px(old_data, (atomicptr_t *)(uintptr_t)ptr);

    if (old_data) {
        // the writer has to take the cost/blocking of providing persistent memory
        struct hzp_free *item = malloc();
        if(!item)
            return false;
        // struct shared_data can contain that memory
        // struct hzp_free *item = &old_data->hzp_member;
        // memory saving trick: aliasing struct hzp_free and a
        // big data member in struct shared_data via a union
        // is possible in specific cases
        hzp_deferfree(item, old_data, shared_data_free);
    }
    return true;
}
```
## adding additional hzp subsystem
add a new named key to the `enum hzps` in `lib/hzp.h` before the HZP_MAX entry.

## Validation Checklist

- [ ] All readers call `hzp_ref()` **before** accessing shared data
- [ ] All readers call `hzp_unref()` **after** last access
- [ ] All writers free old data by using `hzp_deferfree()`
- [ ] `hzp_scan()` called periodically in main thread
- [ ] Thread calls `hzp_alloc()` on start
- [ ] Thread calls `hzp_free()` on exit

## Troubleshooting

| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| Memory leak | Missing `hzp_unref()` | Check all read paths |
| Performance drop | Excessive writer thrashing | only use paradigm for read mostly shared data |
| Data corruption | Compiler/Cpu reordering | Add `volatile` or/and barriers |

## References
- **Hazard Pointers Paper:** "Hazard Pointers: Safe Memory Reclamation for Lock-Free Objects" by Michael (2004)
- **Your Implementation:** `lib/hzp.c`, `lib/hzp.h`
- **Atomic operations:** `lib/reference/ATOMIC_REFERENCE.md` — used by HZP for safe memory reclamation
- **Compiler helpers:** `lib/reference/OTHER_REFERENCE.md` — compiler compatibility macros
- **License:** GPL v3 (see `AGENTS.md` for full license details)

---
*Last Updated: 2026-05-14*

