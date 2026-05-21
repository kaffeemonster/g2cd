# Endianness & Unaligned Access Reference Guide

## Quick Start

To read a 32-bit integer from a potentially unaligned network buffer with conditional endianness:

```c
uint32_t value;
// Read from buffer, swapping if 'big_endian' is true
get_unaligned_endian(&value, (uint32_t *)buffer_ptr, source->big_endian);
```

To read a 16-bit integer from a potentially unaligned network buffer with fixed endianness:

```c
uint32_t value;
// Read from buffer, swapping if host is little endian
get_unaligned_be16(&value, (uint16_t *)buffer_ptr);
```

## Overview

This library provides tools to handle two critical issues when dealing with binary network protocols:
1. **Unaligned Access:** Many architectures (e.g., ARM, PPC, SPARC) trigger a CPU fault (SIGBUS) when accessing multi-byte types from addresses not aligned to their size.
2. **Endianness:** Protocol data may be Big-Endian (Network Order) or Little-Endian, while the host CPU might be either.

## Core Concepts

### 1. Unaligned Access
The `get_unaligned` and `put_unaligned` macros ensure that data is read/written byte-by-byte or using architecture-specific instructions that avoid alignment faults. On x86, these often resolve to direct pointer dereferences as the hardware handles unalignment.

### 2. Endianness (Byte Order)
The library provides semantic wrappers to convert between Host (CPU) and Protocol endianness:
- **BE (Big Endian):** Most significant byte first.
- **LE (Little Endian):** Least significant byte first.

### 3. Swabbing (Raw Swap)
The `__swab` functions perform raw byte-reversal regardless of the current host architecture. They are the primitive building blocks for the endianness layer.

## API Reference

### Fused Helpers
Used when both alignment and endianness are uncertain.

| Macro | Purpose | Notes |
|----------|---------|-------|
| `get_unaligned_endian(dest, ptr, big_end)` | Read unaligned value, swap if `big_end` is true | Most common for protocol parsing |

### Unaligned Helpers
Used when data is known to be unaligned but endianness is already handled.

| Macro | Purpose | Notes |
|----------|---------|-------|
| `get_unaligned(dest, ptr)` | Read unaligned value into `dest` | Architecture-safe |
| `put_unaligned(ptr, val)` | Write value to unaligned `ptr` | Architecture-safe |

### Fixed Endianness Unaligned Helpers
Used when data is known to be unaligned, endianness is known/fixed.

| Macro | Purpose | Notes |
|----------|---------|-------|
| `get_unaligned_be16/32/64(ptr)` | Read unaligned value from `ptr`, converting Big Endian $\rightarrow$ Host | Architecture-safe |
| `put_unaligned_be16/32/64(ptr, val)` | Write value `val` to unaligned `ptr`, converting Host $\rightarrow$ Big Endian | Architecture-safe |
| `get_unaligned_le16/32/64(ptr)` | Read unaligned value from `ptr`, converting Little Endian $\rightarrow$ Host | Architecture-safe |
| `put_unaligned_le16/32/64(ptr, val)` | Write value `val` to unaligned `ptr`, converting Host $\rightarrow$ Little Endian | Architecture-safe |

### Endianness Helpers
Used for semantic conversion between host and specific endianness.

| Macro/Function | Purpose | Notes |
|----------|---------|-------|
| `cpu_to_be32(x)` / `be32_to_cpu(x)` | Host $\leftrightarrow$ Big Endian (32-bit) | Semantic wrapper |
| `cpu_to_le32(x)` / `le32_to_cpu(x)` | Host $\leftrightarrow$ Little Endian (32-bit) | Semantic wrapper |
| `cpu_to_be16(x)` / `be16_to_cpu(x)` | Host $\leftrightarrow$ Big Endian (16-bit) | Semantic wrapper |
| `cpu_to_be64(x)` / `be64_to_cpu(x)` | Host $\leftrightarrow$ Big Endian (64-bit) | Semantic wrapper |

### Swab Helpers
Lowest-level raw unconditional byte reversal.

| Function | Purpose | Notes |
|----------|---------|-------|
| `__swab16(val)` / `__swab32(val)` / `__swab64(val)` | Reverse bytes of a value | Constant folding supported |
| `__swab16p(ptr)` / `__swab32p(ptr)` / `__swab64p(ptr)` | Read value at `ptr`, hard reverse bytes | Assumes `ptr` is naturally aligned |

## Common Pitfalls

### 1. Direct Pointer Dereference
**WRONG:**
```c
uint32_t val = *(uint32_t *)unaligned_ptr; // CRASH on ARM/PPC
```
**FIX:** Use `get_unaligned` or `get_unaligned_endian`.

### 2. Swabbing Aligned Pointers
`__swab32p(ptr)` assumes the pointer is naturally aligned. If the pointer is unaligned, use an unaligned helper.

## Examples

### Protocol Header Parsing
In `G2Packet.c`, when reading a query key from a data trunk where the source's endianness is dynamic:

```c
uint32_t query_key;
get_unaligned_endian(&query_key, (uint32_t *)buffer_start(source->data_trunk), source->big_endian);
```

Or reading an IPv4 address which are always transported in network byte (BE) order:

```c
uint32_t ipv4;
get_unaligned_be32(&ipv4, (uint32_t *)buffer_start(source->data_trunk));
```

### Natural Endian Conversion
Converting a port number from network order (BE) to host order:

```c
// semantically expresses what should happen, swap on LE host, nothing on BE host
uint16_t port = be16_to_cpu(port);
```

### Byte Reversal / Fixed Endian Conversion
hard reversal of bytes in a variable, should map to the best instruction:

```c
// hard reversal of bytes
uint64_t val = __swab(input);
```

## References
- **Implementation:** `lib/unaligned.h`, `lib/swab.h`, `lib/generic/big_endian.h`, `lib/generic/little_endian.h`
- **Compiler Helpers:** `lib/reference/OTHER_REFERENCE.md`
- **License:** GPL v3

---
*Last Updated: 2026-05-17*
