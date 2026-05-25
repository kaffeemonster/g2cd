# Bit-Manipulation and Checksum Reference

This document provides a technical reference for the bit-manipulation, checksum, and memory-utility functions implemented in `lib/my_bitops.h` and `lib/my_bitopsm.h`.

## 1. Checksums

### `adler32`
Computes the Adler-32 checksum of a data stream.
**Signature:**
```c
uint32_t adler32(uint32_t adler, const uint8_t *buf, unsigned len);
```
**Description:**
Calculates a 32-bit checksum using the Adler-32 algorithm. It is designed for speed and efficiency, utilizing different paths based on the input length:
- **Single byte:** Optimized path for `len == 1`.
- **Short sequences:** Uses a common loop for lengths below `COMMON_WORK`.
- **Large sequences:** Uses a vectorized approach (`adler32_vec`) to process data in blocks, leveraging 64-bit registers on supported architectures for pseudo-SIMD performance.
**Parameters:**
- `adler`: The initial checksum value (typically `1` for a new stream).
- `buf`: Pointer to the data buffer.
- `len`: Length of the data in bytes.
**Return Value:**
The resulting 32-bit Adler-32 checksum.
**Complexity:** $O(n)$

## 2. Bit Manipulation

### `popcountst`
Counts the number of bits set to 1 (population count) in a `size_t` value.
**Signature:**
```c
size_t popcountst(size_t n);
ssize_t bitfield_decode(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len);
```
**Description:**
Returns the number of set bits (1s) in the input `size_t` integer. This is a fundamental operation for many bit-manipulation tasks. The implementation is highly optimized and architecture-specific, using native CPU instructions (like `POPCNT` on x86) where available to ensure constant-time or near-constant-time performance.
Reverses the `bitfield_encode` process to reconstruct the original bitfield. It restores the runs of `0xFF` and decodes the specialized prefixes for bytes with few bits cleared.
**Parameters:**
- `n`: The `size_t` value to analyze.
**Return Value:**
The total count of set bits in `n`.
**Complexity:** $O(1)$

### `flsst`
Finds the position of the most significant bit (last set bit) in a `size_t` value.
**Signature:**
```c
size_t flsst(size_t find);
```
**Description:**
Returns the 1-based index of the most significant bit set to 1 in the input `size_t` value. For example, if the input is `1` (binary `...0001`), it returns `1`. If the input is `2` (binary `...0010`), it returns `2`. If the input is `0`, the behavior is typically implementation-defined (often returning 0 or 1 depending on the architecture's BSR/CLZ instruction). This function is highly optimized using native CPU instructions to provide constant-time performance.
**Parameters:**
- `find`: The `size_t` value to analyze.
**Return Value:**
The 1-based index of the most significant set bit.
**Complexity:** $O(1)$

## 3. Sorting
### `introsort_u32`
Sorts an array of 32-bit unsigned integers using the Introsort algorithm, with integrated duplicate removal.
**Signature:**
```c
size_t introsort_u32(uint32_t a[], size_t n);
```
**Description:**
Performs a highly optimized sort of an array of `uint32_t`. Introsort is a hybrid sorting algorithm that starts with Quicksort and switches to Heapsort when the recursion depth exceeds a certain limit, avoiding the $O(n^2)$ worst-case performance of Quicksort. A final pass of Insertion Sort is used to finish the sorting of small partitions.
Additionally, this implementation performs partial duplicate removal during the final sorting phase, reducing the number of elements in the resulting array.
**Parameters:**
- `a`: The array of `uint32_t` to be sorted.
- `n`: The number of elements in the array.
**Return Value:**
The number of unique (or "valid") elements remaining in the array after sorting and duplicate removal.
**Complexity:** $O(n \log n)$

## 4. Bitfield RLE
This section describes a specialized Run-Length Encoding (RLE) scheme optimized for sparsely populated bitfields, typically used for QHT bloom filter compression.

### `bitfield_encode`
Run-length encodes a bitfield.
**Signature:**
```c
ssize_t bitfield_encode(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len);
```
**Description:**
Compresses a bitfield by identifying runs of `0xFF` and using specialized prefixes for bytes with few bits cleared. It is specifically tuned for data where most bits are set (the "background" color in this context is `0xFF`).
The encoding scheme uses:
- **Runs of `0xFF`:** Coded as bytes with the MSB cleared, representing the run length minus one.
- **Sparsely cleared bytes:**
    - One bit cleared: Prefixed with `0xE0` or'ed with the bit index.
    - Two bits cleared: Prefixed with `0xC0` or'ed with an index into a code table.
    - Three or more bits cleared: Prefixed with `0x80` and the run length.
**Parameters:**
- `res`: Buffer to store the RLE result.
- `t_len`: Maximum size of the result buffer.
- `data`: Pointer to the input bitfield data.
- `s_len`: Length of the input data in bytes.
**Return Value:**
Returns the length of the resulting encoded data. If the input data could not be fully consumed (e.g., due to the result buffer being too small), it returns the negative of the result length.
**Complexity:** $O(n)$


### `bitfield_decode`
Decodes a run-length encoded bitfield.

**Signature:**
```c
ssize_t bitfield_decode(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len);
```

**Description:**
Reverses the `bitfield_encode` process to reconstruct the original bitfield. It restores the runs of `0xFF` and decodes the specialized prefixes for bytes with few bits cleared.

**Parameters:**
- `res`: Buffer to store the decoded bitfield.
- `t_len`: Maximum size of the result buffer.
- `data`: Pointer to the RLE encoded data.
- `s_len`: Length of the encoded data in bytes.

**Return Value:**
Returns the length of the resulting decoded data. If the encoded data could not be fully consumed (e.g., due to the result buffer being too small), it returns the negative of the result length.

**Complexity:** $O(n)$

### `bitfield_and`
Performs a bitwise AND between a run-length encoded bitfield and an uncompressed bitfield.

**Signature:**
```c
ssize_t bitfield_and(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len);
```

**Description:**
Ands the encoded bitfield (in `data`) with the uncompressed bitfield (in `res`). The result is stored back in `res`. This avoids full decompression of the encoded bitfield by applying the AND operation directly during the decoding process.

**Parameters:**
- `res`: Buffer containing the uncompressed bitfield; also stores the result.
- `t_len`: Size of the buffer.
- `data`: Pointer to the RLE encoded bitfield.
- `s_len`: Length of the encoded data in bytes.

**Return Value:**
Returns the length of the resulting bitfield. If the encoded data could not be fully consumed, it returns the negative of the result length.

**Complexity:** $O(n)$

### `bitfield_lookup`
Tests a set of bit indexes against a run-length encoded bitfield.

**Signature:**
```c
int bitfield_lookup(const uint32_t *vals, size_t v_len, const uint8_t *data, size_t s_len);
```

**Description:**
Checks if any of the bit indexes specified in `vals` are set (cleared to 0 in the context of the bitfield's "background" color) in the encoded bitfield. This is the most efficient way to query a compressed bitfield as it "walks" the RLE data without full decompression.

**Note:** This function is most efficient when `vals` is sorted in ascending order.

**Parameters:**
- `vals`: Array of bit indexes to look up.
- `v_len`: Number of elements in the `vals` array.
- `data`: Pointer to the RLE encoded bitfield.
- `s_len`: Length of the encoded data in bytes.

**Return Value:**
Returns `-1` if at least one index matched (bit is cleared), and `0` otherwise.

**Complexity:** $O(n)$ (where $n$ is the distance to the target bit in the encoded stream)

## 5. Memory Operations

### `my_memcpy`
Copies a block of memory from source to destination.
**Signature:**
```c
void *my_memcpy(void *restrict dst, const void *restrict src, size_t len);
```
**Description:**
A highly optimized, portable memory copy implementation. It employs a multi-tiered dispatch strategy based on the length of the data and detected CPU features:
- **Small copies (< 16 bytes):** Uses a fast-path `cpy_rest_o` implementation.
- **Medium copies (16 bytes to 512 bytes):** Uses `memcpy_small`, which leverages `rep movsb`/`rep movsq` on x86 architectures for efficient small-to-medium transfers.
- **Mid-range copies (512 bytes to 256 KB):** Dispatches to `my_memcpy_medium`, which is optimized using SIMD instructions (SSE, SSE2, SSE3, 3DNow) based on the processor's capabilities.
- **Large copies (>= 256 KB):** Dispatches to `my_memcpy_big`, utilizing non-temporal stores or larger block transfers to maximize throughput and minimize cache pollution.
**Parameters:**
- `dst`: Pointer to the destination memory region.
- `src`: Pointer to the source memory region.
- `len`: Number of bytes to copy.
**Return Value:**
Returns a pointer to the destination region `dst`.
**Complexity:** $O(n)$

### `my_mempcpy`
Copies a block of memory and returns a pointer to the end of the copied region.
**Signature:**
```c
void *my_mempcpy(void *restrict dst, const void *restrict src, size_t len);
```
**Description:**
Similar to `my_memcpy`, but instead of returning the start of the destination buffer, it returns a pointer to the byte immediately following the last byte copied. This is extremely useful for concatenating multiple memory blocks without repeatedly calculating the current offset.
**Parameters:**
- `dst`: Pointer to the destination memory region.
- `src`: Pointer to the source memory region.
- `len`: Number of bytes to copy.
**Return Value:**
A pointer to the end of the copied region (`dst + len`).
**Complexity:** $O(n)$

### `my_memcpy_fwd`
Forward memory copy (Alias of `my_memcpy`).

**Signature:**
```c
void *my_memcpy_fwd(void *dst, const void *src, size_t len);
```
**Description:**
Provided as an explicit alias to `my_memcpy` to indicate a forward copy operation.

**Complexity:** $O(n)$

### `my_memcpy_rev`
Reverse memory copy.
**Signature:**
```c
void *my_memcpy_rev(void *dst, const void *src, size_t len);
```
**Description:**
Copies memory from source to destination in reverse order (from the end of the source to the beginning of the destination). This is useful for avoiding overlap issues when the destination starts after the source.

**Parameters:**
- `dst`: Pointer to the destination memory region.
- `src`: Pointer to the source memory region.
- `len`: Number of bytes to copy.
**Return Value:**
Returns a pointer to the destination region `dst`.
**Complexity:** $O(n)$

### `my_memmove`
Copies a block of memory, handling overlapping source and destination regions.
**Signature:**
```c
void *my_memmove(void *dst, const void *src, size_t len);
```
**Description:**
Copies `len` bytes from `src` to `dst`. Unlike `my_memcpy`, `my_memmove` safely handles cases where the source and destination regions overlap. It achieves this by analyzing the relative positions of `src` and `dst`:
- If `dst` is before `src`, atau `dst` is before `src`, it performs a forward copy using `my_memcpy_fwd`.
- If `dst` is after `src`, it performs a reverse copy using `my_memcpy_rev` to avoid overwriting the source data before it is read.
**Parameters:**
- `dst`: Pointer to the destination memory region.
- `src`: Pointer to the source memory region.
- `len`: Number of bytes to copy.
**Return Value:**
Returns a pointer to the destination region `dst`.
**Complexity:** $O(n)$

### `my_memchr`
Searches for the first occurrence of a byte in a memory region.
**Signature:**
```c
void *my_memchr(const void *s, int c, size_t n);
```
**Description:**
Scans the first `n` bytes of the memory area starting at `s` for the first occurrence of the byte `c`. This implementation is optimized across different architectures (x86, ARM, MIPS, etc.), often using SIMD or word-at-a-time techniques to improve search speed.
**Parameters:**
- `s`: Pointer to the memory region to search.
- `c`: The byte value to search for.
- `n`: The number of bytes to examine.
**Return Value:**
Returns a pointer to the first occurrence of the byte `c` in the region, or `NULL` if the byte is not found.
**Complexity:** $O(n)$

## 6. Memory-wide Bitwise Operations
This section describes functions that apply bitwise operations across entire blocks of memory, typically utilizing SIMD instructions for high throughput.

### `memxorcpy`
XORs two memory regions and copies the result to a destination.
**Signature:**
```c
void *memxorcpy(void *dst, const void *src1, const void *src2, size_t len);
```
**Description:**
Computes the bitwise XOR of `src1` and `src2` and stores the result in `dst`. This is often used in checksum calculations or for comparing data streams. The implementation is highly optimized with architecture-specific paths (e.g., AVX, SSE, MMX on x86) to process multiple bytes per cycle.
**Parameters:**
- `dst`: Pointer to the destination memory region.
- `src1`: Pointer to the first source memory region.
- `src2`: Pointer to the second source memory region.
- `len`: Number of bytes to process.
**Return Value:**
Returns a pointer to the destination region `dst`.
**Complexity:** $O(n)$

### `memand`
Performs a bitwise AND between two memory regions and copies the result to a destination.
**Signature:**
```c
void *memand(void *dst, const void *src, size_t len);
```
**Description:**
Ands the memory region at `src` with the memory region at `dst` and stores the result back in `dst`. Like `memxorcpy`, this is optimized using SIMD instructions where available.
**Parameters:**
- `dst`: Pointer to the destination memory region (also acts as the first source).
- `src`: Pointer to the second source memory region.
- `len`: Number of bytes to process.
**Return Value:**
Returns a pointer to the destination region `dst`.
**Complexity:** $O(n)$

### `memneg`
Performs a bitwise NOT (negation) on a memory region.
**Signature:**
```c
void *memneg(void *dst, const void *src, size_t len);
```
**Description:**
Computes the bitwise NOT of the region at `src` and stores the result in `dst`.
**Parameters:**
- `dst`: Pointer to the destination memory region.
- `src`: Pointer to the source memory region.
- `len`: Number of bytes to process.
**Return Value:**
Returns a pointer to the destination region `dst`.
**Complexity:** $O(n)$

### `mempopcnt`
Counts the total number of set bits (1s) in a memory region.
**Signature:**
```c
size_t mempopcnt(const void *s, size_t len);
```
**Description:**
Calculates the population count (number of set bits) across the entire memory block of length `len`. This is used for analyzing the density of bitfields or bloom filters.
**Parameters:**
- `s`: Pointer to the memory region to analyze.
- `len`: Length of the memory region in bytes.
**Return Value:**
The total number of bits set to 1.
**Complexity:** $O(n)$

## 7. Memory and String Scanning
This section describes utility functions for scanning memory and strings for specific patterns or spans.

### `mem_searchrn`
Finds the first occurrence of a CRLF (`\r\n`) sequence in a memory region.
**Signature:**
```c
void *mem_searchrn(void *src, size_t len);
```
**Description:**
Scans the memory region starting at `src` for the first occurrence of the carriage return (`\r`) followed by a line feed (`\n`) sequence. It is highly optimized across architectures, using SIMD (AVX, SSE) on x86 to search for the sequence in parallel.
**Parameters:**
- `src`: Pointer to the memory region to search.
- `len`: Length of the region in bytes.
**Return Value:**
Returns a pointer to the start of the first `\r\n` sequence found, or `NULL` if not found.
**Complexity:** $O(n)$

### `mem_spn_ff`
Counts the length of a span of bytes set to `0xFF` at the start of a memory region.
**Signature:**
```c
size_t mem_spn_ff(const void *src, size_t len);
```
**Description:**
Returns the number of consecutive bytes equal to `0xFF` starting from the beginning of the memory region. This is particularly useful for decoding RLE-compressed bitfields. Optimized implementations use SIMD (AVX2, SSE) to check blocks of bytes.
**Parameters:**
- `src`: Pointer to the memory region.
- `len`: Length of the region in bytes.
**Return Value:**
The number of consecutive `0xFF` bytes.
**Complexity:** $O(n)$

### `str_spn_space`
Counts the length of a span of whitespace characters at the start of a string.
**Signature:**
```c
size_t str_spn_space(const char *str);
```
**Description:**
Counts the number of leading whitespace characters (spaces, tabs, etc.) in the provided string.
**Parameters:**
- `str`: The string to analyze.
**Return Value:**
The number of leading whitespace characters.
**Complexity:** $O(n)$

### `str_skip_space`
Skips the leading whitespace of a string and returns a pointer to the first non-whitespace character.
**Signature:**
```c
char *str_skip_space(char *str);
```
**Description:**
A convenience wrapper around `str_spn_space` that returns the pointer to the first non-whitespace character in the string.
**Parameters:**
- `str`: The string to process.
**Return Value:**
A pointer to the first non-whitespace character, or the end of the string if it consists entirely of whitespace.
**Complexity:** $O(n)$

## 8. String Utility Functions
This section describes specialized string operations for efficient manipulation and comparison.

### `strncasecmp_a`
Performs a case-insensitive comparison of two strings (ASCII only).
**Signature:**
```c
int strncasecmp_a(const char *s1, const char *s2, size_t n);
```
**Description:**
Compares up to `n` characters of two strings, ignoring case. This function is restricted to the ASCII character set and is highly optimized using SIMD instructions (AVX, SSE) on x86 to process multiple characters per cycle.
**Parameters:**
- `s1`: First string.
- `s2`: Second string.
- `n`: Maximum number of characters to compare.
**Return Value:**
An integer less than, equal to, or greater than zero if `s1` is found to be less than, equal to, or greater than `s2`.
**Complexity:** $O(n)$

### `strpcpy`
Copies a string and returns the pointer to the end of the copied region.
**Signature:**
```c
char *strpcpy(char *restrict dst, const char *restrict src);
```
**Description:**
Copies the string pointed to by `src` to the buffer pointed to by `dst`. Unlike `strcpy`, it returns a pointer to the null terminator of the destination string. This allows for highly efficient string concatenation in a loop without needing to re-calculate the string length.
**Parameters:**
- `dst`: Destination buffer.
- `src`: Source string.
**Return Value:**
A pointer to the end of the copied string in `dst`.
**Complexity:** $O(n)$

### `strlpcpy`
Copies a string with a length limit and returns the pointer to the end of the copied region.
**Signature:**
```c
char *strlpcpy(char *restrict dst, const char *restrict src, size_t maxlen);
```
**Description:**
Similar to `strpcpy`, but ensures that no more than `maxlen` bytes are copied. It returns the pointer to the end of the copied region.
**Parameters:**
- `dst`: Destination buffer.
- `src`: Source string.
- `maxlen`: Maximum number of bytes to copy.
**Return Value:**
A pointer to the end of the copied string in `dst`.
**Complexity:** $O(n)$

## 9. Specialized Memory Copies
This section describes utilities for copying small "trailers" or remnants of memory blocks with high efficiency.

### `cpy_rest`
Copies a small number of bytes (up to 15) from source to destination.
**Signature:**
```c
char *cpy_rest(char *dst, const char *src, unsigned i);
```
**Description:**
An optimized function for copying the remaining bytes of a memory block. It uses a switch-case dispatch to copy data using the largest possible unaligned word sizes (up to 64-bit) to minimize instructions. It is typically used as a "cleanup" step after a vectorized copy loop.
**Parameters:**
- `dst`: Destination buffer.
- `src`: Source buffer.
- `i`: Number of bytes to copy (typically $0 \le i < 16$).
**Return Value:**
Returns a pointer to the end of the copied region (`dst + i`).
**Complexity:** $O(1)$ (for the fixed range $0-15$)

### `cpy_rest_o`
Copies a small number of bytes and returns the start of the destination.
**Signature:**
```c
char *cpy_rest_o(char *dst, const char *src, unsigned i);
```
**Description:**
Identical to `cpy_rest` in its copying logic, but returns the original destination pointer `dst` instead of the end pointer.
**Parameters:**
- `dst`: Destination buffer.
- `src`: Source buffer.
- `i`: Number of bytes to copy.
**Return Value:**
Returns the pointer to the start of the destination region `dst`.
**Complexity:** $O(1)$

### `cpy_rest0`
Copies a small number of bytes and ensures null-termination.
**Signature:**
```c
char *cpy_rest0(char *dst, const char *src, unsigned i);
```
**Description:**
Copies `i` bytes from `src` to `dst` and explicitly sets the byte at `dst[i]` to `\0`. This is used when the resulting copy must be a valid null-terminated string.
**Parameters:**
- `dst`: Destination buffer.
- `src`: Source buffer.
- `i`: Number of bytes to copy.
**Return Value:**
Returns a pointer to the end of the copied region (`dst + i`).
**Complexity:** $O(1)$

## 10. Portable String Fallbacks
This section describes portable implementations of standard string functions, designed to provide consistent behavior across different platforms and architectures, often utilizing word-at-a-time optimizations for performance.

### `strlen`
Calculates the length of a null-terminated string.
**Signature:**
```c
size_t strlen(const char *s);
```
**Description:**
A highly optimized implementation that processes the string in blocks of `SOST` bytes (typically 4 or 8 bytes depending on architecture) using the `has_nul_byte` and `nul_byte_index` utilities. It avoids byte-by-byte scanning to significantly improve performance on most platforms.
**Parameters:**
- `s`: Pointer to the null-terminated string.
**Return Value:**
The number of bytes preceding the null terminator.
**Complexity:** $O(n)$

### `strnlen`
Calculates the length of a string, with a maximum limit.
**Signature:**
```c
size_t strnlen(const char *s, size_t maxlen);
```
**Description:**
Similar to `strlen`, but stops scanning once it reaches `maxlen` bytes. It uses the same word-at-a-time optimization strategy but includes logic to ensure that the length is clamped to `maxlen` and that memory is not read beyond the limit in a way that would cause a page fault.
**Parameters:**
- `s`: Pointer to the string.
- `maxlen`: Maximum number of bytes to scan.
**Return Value:**
The length of the string, or `maxlen` if no null terminator was found within the first `maxlen` bytes.
**Complexity:** $O(n)$

### `strchrnul`
Locates the first occurrence of a character or the null terminator.
**Signature:**
```c
char *strchrnul(const char *s, int c);
```
**Description:**
Searches for the first occurrence of character `c` in string `s`. If `c` is 0 (null terminator), it returns a pointer to the end of the string. The implementation uses `has_eq_byte` to detect the target character and `has_nul_byte` to detect the end of the string simultaneously using word-sized loads.
**Parameters:**
- `s`: Pointer to the string.
- `c`: The character to search for.
**Return Value:**
A pointer to the first occurrence of the character `c`, or a pointer to the null terminator if `c` is not found.
**Complexity:** $O(n)$

### `strrchr`
Locates the last occurrence of a character before the null terminator.
**Signature:**
```c
char *strrchr(const char *s, int c);
```
**Description:**
Searches for the last occurrence of character `c` in string `s`. Instead of searching backwards from the end, it performs a forward walk, keeping track of the most recent match found. This approach is often more efficient on modern CPUs due to better prefetching and linear memory access.
**Parameters:**
- `s`: Pointer to the string.
- `c`: The character to search for.
**Return Value:**
A pointer to the last occurrence of the character `c`, or `NULL` if the character is not found.
**Complexity:** $O(n)$

## 11. Base Conversion Utilities
This section describes functions for encoding binary data into human-readable string representations (hexadecimal and base32).

### `to_base16`
Converts a binary string to a hexadecimal representation.
**Signature:**
```c
unsigned char *to_base16(unsigned char *dst, const unsigned char *src, unsigned len);
```
**Description:**
Encodes each byte of the source binary data into two hexadecimal characters (`0-9`, `a-f`). The implementation is optimized to process the source in blocks of `SOST` bytes, utilizing word-sized loads and arithmetic to generate the hex characters efficiently.
**Parameters:**
- `dst`: Pointer to the destination buffer where the hex string will be stored.
- `src`: Pointer to the source binary data.
- `len`: Number of bytes to encode.
**Return Value:**
Returns a pointer to the end of the resulting hexadecimal string in `dst`.
**Complexity:** $O(n)$

### `to_base32`
Converts a binary string to a base32 representation.
**Signature:**
```c
unsigned char *to_base32(unsigned char *dst, const unsigned char *src, unsigned len);
```
**Description:**
Encodes binary data into a base32 string (representing 5 bits per character). The implementation uses a sophisticated "do_40bit" logic to group 5-bit quantities and convert them into characters. It also handles the standard base32 padding with `=` characters at the end of the resulting string to ensure the output length is a multiple of 8.
**Parameters:**
- `dst`: Pointer to the destination buffer where the base32 string will be stored.
- `src`: Pointer to the source binary data.
- `len`: Number of bytes to encode.
**Return Value:**
Returns a pointer to the end of the resulting base32 string in `dst`.
**Complexity:** $O(n)$

## 12. Specialized String Transformations
This section describes utilities for reversing strings and decoding HTML entities.

### `strreverse_l`
Reverses a string in-place between two pointers.
**Signature:**
```c
void strreverse_l(char *begin, char *end);
```
**Description:**
Reverses the sequence of characters in the memory range `[begin, end)`. To maximize performance, the implementation uses a tiered strategy: it first swaps 64-bit words, then 32-bit words, then 16-bit words, and finally handles the remaining bytes individually. This significantly reduces the number of iterations compared to a simple byte-by-byte swap. Architecture-specific optimizations (such as AVX2, AVX, and SSE on x86) are used to further accelerate the process.
**Parameters:**
- `begin`: Pointer to the start of the string to reverse.
- `end`: Pointer to the end of the string to reverse.
**Complexity:** $O(n)$

### `decode_html_entities_utf8`
Converts HTML entities to their UTF-8 representation.
**Signature:**
```c
size_t decode_html_entities_utf8(char *dest, const char *src, size_t len);
```
**Description:**
Parses a string and replaces HTML entities (e.g., `&amp;`, `&#x20AC;`, `&#8364;`) with their corresponding UTF-8 encoded characters. It utilizes a comprehensive lookup table for named entities and handles both decimal and hexadecimal numeric entities. The result is written to the `dest` buffer.
**Parameters:**
- `dest`: Pointer to the destination buffer for the decoded string.
- `src`: Pointer to the source string containing HTML entities.
- `len`: Length of the source string.
**Return Value:**
The length of the resulting decoded UTF-8 string.
**Complexity:** $O(n)$

## 13. CPU Feature Detection and Patching
This section describes utilities for detecting CPU capabilities and dynamically patching instructions to use optimized paths.

### `struct test_cpu_feature`
Used to define a CPU feature test and its associated flags.
**Fields:**
- `func`: A function pointer to the test logic.
- `flags`: Bitmask of flags (e.g., `CFF_AVX_TST`).
- `features`: (x86 only) An array of CPU feature identifiers.

### `test_cpu_feature`
Tests for the presence of a specific CPU feature.
**Signature:**
```c
void *test_cpu_feature(const struct test_cpu_feature *, size_t);
```
**Description:**
Evaluates the provided `test_cpu_feature` structure to determine if the current CPU supports the targeted feature.
**Parameters:**
- `t`: Pointer to the feature test definition.
- `l`: Length or index related to the feature set being tested.
**Return Value:**
A pointer to the resulting feature or a status indicator.

### `patch_instruction`
Dynamically patches a binary instruction.
**Signature:**
```c
void patch_instruction(void *where, const struct test_cpu_feature *t, size_t l);
```
**Description:**
Replaces an instruction at the specified memory address with an optimized version if the feature test `t` passes.
**Parameters:**
- `where`: The address of the instruction to patch.
- `t`: The feature test structure.
- `l`: The length of the patch.

### `patch_got`
Patches a Global Offset Table (GOT) entry.
**Signature:**
```c
void patch_got(uintptr_t *where, const struct test_cpu_feature *t, size_t l);
```
**Description:**
Updates a GOT entry to point to an optimized implementation of a function if the CPU feature test passes.
**Parameters:**
- `where`: The address of the GOT entry.
- `t`: The feature test structure.
- `l`: The length/index of the patch.

### `emit_emms`
Emits the `EMMS` (Empty Maximum SIMD State) instruction.
**Signature:**
```c
void emit_emms(void);
```
**Description:**
Ensures that the FPU tag word is reset, which is necessary after using SIMD instructions to avoid precision issues with floating-point operations.

### `get_cpus_online`
Returns the number of online CPUs.
**Signature:**
```c
unsigned get_cpus_online(void);
```
**Description:**
Queries the system to find the number of active CPU cores.
**Return Value:**
The number of online CPUs.

### `cpu_detect_finish`
Finalizes CPU detection.
**Signature:**
```c
void cpu_detect_finish(void);
```
**Description:**
Performs final cleanup or registration after the CPU feature detection phase is complete.

## 14. Bit Manipulation and Alignment Helpers
This section describes static inline functions and macros for bit-level operation and memory alignment.

### `roundup_power_of_2`
Rounds up a value to the next power of two.
**Signature:**
```c
static inline size_t roundup_power_of_2(size_t c);
```
**Description:**
Calculates the smallest power of two that is greater than or equal to the input value `c`. It uses a series of bit-shifting and OR operations to fill all bits to the right of the most significant bit, then adds 1.
**Parameters:**
- `c`: The value to round up.
**Return Value:**
The next power of two.
**Complexity:** $O(1)$

### Alignment and Rounding Macros
These macros provide efficient ways to align pointers and sizes to power-of-two boundaries.

- `ROUND_ALIGN(x, n)`: Rounds `x` up to the nearest multiple of `n` (where `n` is a power of 2).
- `ROUND_TO(x, n)`: Rounds `x` down to the nearest multiple of `n` (where `n` is a power of 2).
- `IS_ALIGN(x, n)`: Checks if the pointer or value `x` is aligned to a boundary of `n`.
- `ALIGN(x, n)`: Aligns the pointer `x` up to the next boundary of `n`.
- `ALIGN_DIFF(x, n)`: Calculates the number of bytes needed to align `x` to the next boundary of `n`.
- `ALIGN_SIZE(x, n)`: Rounds the size `x` up to match the alignment `n`.
- `ALIGN_DOWN(x, n)`: Aligns the pointer `x` down to the previous boundary of `n`.
- `ALIGN_DOWN_DIFF(x, n)`: Calculates the offset from the previous boundary of `n`.
- `DIV_ROUNDUP(a, b)`: Performs division `a / b` and rounds the result up to the nearest integer.
- `B32_LEN(x)`: Calculates the required output buffer length for a base32 encoded string of length `x`.


