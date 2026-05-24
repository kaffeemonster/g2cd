# TCHAR (Windows Compatibility) Reference Guide

## Quick Start

1. Use `tchar_t` only when you need to replicate Windows-specific widecharacter string behavior (e.g., for hashing or search).
2. Convert 8-bit data (UTF-8) to `tchar_t` using `utf8totcs()` when entering a "Windows-parity" critical path.
3. Use `tchar`-specific string functions: `tstrlen()`, `tstrncmp()`, `tstrchrnul()`.
4. For case-insensitive normalization, use `tstrptolower()` to mimic the Windows case-folding logic.
5. Use `tischaracter()` or `tisdigit()` for classification.

## Overview

The **TCHAR** system is not a general-purpose UTF-16 implementation, but a **compatibility layer** designed to replicate the exact string behavior of the Windows API.

This is critical because the Gnutella 2 reference implementation (**Shareaza**) was built on Windows and inherited specific Windows `wchar_t` behaviors (originally UCS-2, then UTF-16) in its hashing, token-stemming, and search algorithms. Because G2CD must match the same hash results to interact correctly with the network's Bloom filters (e.g., in `G2QHT.c`'s `g2_qht_search_drive`), it must mimic these Windows-specific behaviors exactly.

## Historical Context: The "Windows Snafu"

To understand why `tchar_t` exists, one must understand the divergence in Unicode adoption:

- **The Windows Path:** Windows moved early to Unicode using 16-bit `wchar_t`. This led to the famous bifurcation of the Win32 API into **A** (ANSI/8-bit) and **W** (Wide/16-bit) versions of almost every function.
- **The POSIX Path:** POSIX systems adopted Unicode later, often choosing 32-bit `wchar_t`. However, by the time this happened, UTF-8 had already become the dominant standard for 8-bit representations, leaving 32-bit `wchar_t` as an under-supported "step-child".
- **The Result:** Shareaza implemented its core logic using the Windows `W` APIs. This means the "mathematical result" of a search or a hash in G2 is tied to how Windows handles 16-bit characters. 

G2CD uses `tchar_t` to deliver the minimal set of functionality required to achieve this behavioral parity, ensuring we can match against the bloom-filter-sets sent by clients.

## Architecture

- **Behavioral Parity:** Mimics the Windows `wchar_t` (UTF-16) environment.
- **Classification Tables:** Uses pre-computed tables (`tchar_c1table`, `tchar_c3table`, `tchar_tolower`) to ensure classification and case-folding match the Windows API.
- **Optimized Kernels:** String operations (`tstrlen`, `tstrncmp`, `tstrchrnul`) are implemented with architecture-specific ASM optimizations to handle 16-bit characters efficiently.
- **Conversion Bridge:** `utf8totcs` provides the bridge from the 8-bit wire format (UTF-8) to the 16-bit internal representation required for parity calculations.

## Core Concepts

### 1. UTF-8 to Windows-Parity Conversion
When a string must be hashed or searched using the same logic as Shareaza:
```c
tchar_t buffer[256];
size_t src_len = strlen(utf8_input);
/* src_len is in/out, so partial reads can be handler */
size_t written = utf8totcs(buffer, sizeof(buffer)/sizeof(tchar_t), utf8_input, &src_len);
```

### 2. Character Classification
The system replicates the MSDN `GetStringTypeExW` specifications via:
- **CTYPE1:** Basic categories (Upper, Lower, Digit, Space, Punctuation, Control).
- **CTYPE3:** Script-specific attributes (Hiratana, Katakana, Ideograph, Symbol).

### 3. Case Folding (Normalization)
`tstrptolower()` is not just a "lowercase" function; it is a **normalization** tool. It includes specific fixes (like the Greek final sigma `0x03A3` $\rightarrow$ `0x03C2` at word ends) to ensure the resulting string is identical to one produced by the Windows API.

## API Reference

| Function | Purpose | Notes |
|----------|---------|-------|
| `utf8totcs(dst, dl, src, sl)` | Convert 8-bit to Windows-parity 16-bit | The entry point for parity-critical paths |
| `tstrlen(s)` | Get string length | Returns count of `tchar_t` |
| `tstrncmp(s1, s2, n)` | Compare two strings | Up to `n` characters |
| `tstrchrnul(s, c)` | Find char or return `\0` | Returns pointer to match or terminator |
| `tstrptolower(s)` | Windows-style lowercase | Essential for search normalization |
| `tistypemix(s, len, word, digit, mix)` | Detect content type | Identifies if string is pure word, pure digit, or mixed |
| `tctype1(c)` | Get CTYPE1 flags | See `enum tchar_ctype1` |
| `tctype3(c)` | Get CTYPE3 flags | See `enum tchar_ctype3` |

## Common Pitfalls

### 1. Misunderstanding the Purpose
**WRONG:** Thinking `tchar_t` is for general-purpose Unicode support in the server.
**FIX:** Use 8-bit strings for most operations. Only use `tchar_t` when you are implementing logic that must match the G2 network's hash/search results (e.g., the `g2_qht_search_drive` flow).

### 2. Using `strlen()` or `strcmp()`
**WRONG:** Using standard `libc` functions on `tchar_t` strings.
**FIX:** Always use `tstrlen()`, `tstrncmp()`, etc. Standard functions treat `uint16_t` as two `char`s and will  break on the upper byte being null.

### 3. Buffer Size Calculation
Remember that `tchar_t` is 2 bytes. A buffer of `tchar_t buf[100]` has a capacity of 100 characters, but `sizeof(buf)` is 200 bytes.

## Performance Guidelines

- **Limit Scope:** Keep the use of `tchar_t` restricted to the parity-critical paths to avoid unnecessary conversion overhead.
- **Prefer In-place Operations:** Use `tstrptolower` on a temporary buffer to avoid repeated conversions.
- **Leverage ASM:** The `tstr*` functions are heavily optimized for 16-bit widths.

## Validation Checklist

- [ ] Is this logic required to match Shareaza/Windows hashes? (If no, use 8-bit strings).
- [ ] Data is converted via `utf8totcs` before entering the parity path.
- [ ] `tstrlen` and `tstrncmp` are used instead of `strlen` and `strncmp`.
- [ ] Case-insensitive searches use `tstrptolower` normalization.

## References
- **Critical Path Example:** See `G2QHT.c` function `g2_qht_search_drive` and its callees.
- **Source Code:** `lib/tchar.c`, `lib/tchar.h`
- **Optimizations:** `lib/tstrlen.c`, `lib/tstrncmp.c`, `lib/tstrchrnul.c`
- **MSDN Reference:** `GetStringTypeExW` (for CTYPE logic)
