# Cryptography & Hashing Reference Guide

## CRITICAL WARNING

**None of the modules in this section are designed for cryptographic security.**

The AES implementation is used exclusively as a **bit-perturbation engine** for the RNG and GUID generation — NOT for encryption of communication channels, data at rest, or any security-sensitive purpose. The ANSI PRNG is explicitly **non-conforming** to any current standard (X9.31 deprecated by NIST in 2011, non-FIPS since 2016).

**Do NOT** use these modules for real cryptography. They are not audited, not FIPS-compliant, and not designed for cryptographic correctness.

## Overview

The cryptography & hashing modules serve three purposes:

1. **Randomness** — PRNG feeds hash table seeds, salts, sequence numbers, and GUIDs
2. **Identification** — GUID generation for unique node identifiers
3. **Hashing** — Fast hash functions for internal data structures (hash tables)
4. **Checksums** — Adler32 for data integrity verification

## Modules

| Module | Files | Purpose |
|--------|-------|---------|
| AES | `aes.*`, `*/aes.c` | Bit-perturbation engine (not real crypto) |
| ANSI PRNG | `ansi_prng.*` | Non-conforming PRNG (X9.31-based, deprecated) |
| GUID | `guid.*` | Random GUID generation with AES mixing |
| hthash | `hthash.h` | MurmurHash3 or jhash for hash tables |
| Adler32 | `adler32.*`, `*/adler32.c` | Vectorized checksum for zlib |

---

## AES (`aes.*`)

### Purpose

**Bit-perturbation engine only.** Used exclusively as a mixing primitive by the ANSI PRNG and GUID generator. NOT for encrypting data, NOT for secure communication.

### Architecture

Dispatches to architecture-specific implementations via `aes.c` which includes the appropriate `*/aes.c` based on compile-time detection. Falls back to `generic/aes.c` when no optimized path is available.

Supported architectures: `x86/`, `x86_64/`, `arm/`, `ppc/`, `sparc/`, `mips/`, `alpha/`, `ia64/`, `tile/`, `parisc/`, `riscv/`, `generic/`.

Lookup table data files (`aes_ft_tab.bin`, `aes_it_tab.bin`, `aes_fl_tab.bin`, `aes_il_tab.bin`) are generated at build time by `aes_tab_gen.c`.

### API

| Function | Signature | Purpose |
|----------|-----------|---------|
| `aes_encrypt_key128` | `void aes_encrypt_key128(struct aes_encrypt_ctx *, const void *)` | Set up 128-bit key schedule |
| `aes_encrypt_key256` | `void aes_encrypt_key256(struct aes_encrypt_ctx *, const void *)` | Set up 256-bit key schedule |
| `aes_ecb_encrypt128` | `void aes_ecb_encrypt128(const struct aes_encrypt_ctx *, void *, const void *)` | Encrypt single 128-bit block (ECB) |
| `aes_ecb_encrypt256` | `void aes_ecb_encrypt256(const struct aes_encrypt_ctx *, void *, const void *)` | Encrypt single 256-bit block (ECB) |

### Context

```c
struct aes_encrypt_ctx {
    uint32_t k[AES_MAX_KEY_LEN / 4];  /* Max 15 × 16 bytes = 60 uint32s */
} GCC_ATTR_ALIGNED(16);
```

The context is 16-byte aligned. Key schedule stores up to AES-256 (15 rounds).

### Usage

- **`ansi_prng.c`**: Uses `aes_ecb_encrypt256` with AES-256 key for the PRNG core loop (3 encryption calls per refresh cycle)
- **`guid.c`**: Uses `aes_ecb_encrypt256` to mix random bytes with an IV for GUID generation
- **NOT** used anywhere else for encryption

### Pitfalls

- Counter increment in `ansi_prng.c` is **endianness-dependent** — the counter bit pattern differs between little and big endian hosts. Internal-only, not a portability issue for inter-host communication.
- ECB mode only — no block chaining, no IV management. Intentional for bit-perturbation use case.

---

## ANSI PRNG (`ansi_prng.*`)

### Purpose

Pseudo-random number generator inspired by ANSI X9.31 Appendix A.2.4 (AES variant). **NOT cryptographically secure.** Used for internal randomness: hash seeds, salts, sequence numbers, GUIDs.

### WARNING

- X9.31 deprecated by NIST in 2011
- Non-FIPS-compliant since 2016
- Known state-recovery vulnerability (1998)
- NOT a CPRNG (Cryptographically Secure PRNG)
- Do NOT copy this code for cryptographic use

### Architecture

One AES key with three state vectors:
- `DT` — data encryption value (counter, advances each cycle)
- `V` — value state (updated each cycle)
- `rand_data` — output buffer (16 bytes per cycle)

Each refresh cycle performs 3 AES-256 ECB encryptions:
1. `I = AES(DT)` — intermediate value
2. `rand_data = AES(I ⊕ V)` — output
3. `V = AES(rand_data ⊕ I)` — state update

Counter advances by `adler ⊕ timestamp` each cycle. Adler32 checksum tracks intermediate values for additional perturbation.

### API

| Function | Signature | Purpose |
|----------|-----------|---------|
| `random_bytes_init` | `void random_bytes_init(const char[32])` | Initialize PRNG with 32-byte seed |
| `random_bytes_rekey` | `void random_bytes_rekey(const char[32])` | Rekey PRNG with 32-byte data (3 AES keys + IV) |
| `random_bytes_get` | `void random_bytes_get(void *ptr, size_t len)` | Get random bytes (up to 16 per call, refills internally) |

### Seeding

- Initial seed from `/dev/urandom` (or platform equivalent) at startup
- Constantly reseeded during operation via `random_bytes_rekey()` from external entropy
- `get_timestamp()` uses RDTSC on x86, `gettimeofday()` fallback on other architectures

### Consumers

| Consumer | Usage |
|----------|-------|
| `G2ConRegistry.c` | Hash table seed |
| `G2KHL.c` | Cache hash seed, random name length |
| `G2QueryKey.c` | Salt generation, hash seed, active salt |
| `G2UDP.c` | Cache hash seed, sequence seed |
| `G2GUIDCache.c` | Hash table seed |
| `guid.c` | Raw random bytes for GUID generation, key re-entropy |

---

## GUID (`guid.*`)

### Purpose

Generates RFC 4122 version 4 (random) GUIDs for unique node identification. Each GUID is 16 bytes.

### API

| Function | Signature | Purpose |
|----------|-----------|---------|
| `guid_init` | `void guid_init(void)` | Initialize GUID generator (mutex + first key) |
| `guid_generate` | `void guid_generate(unsigned char out[16])` | Generate one random GUID |
| `guid_tick` | `void guid_tick(void)` | Rotate AES key and IV |
| `guid_hash` | `uint32_t guid_hash(const union guid_fast *, uint32_t seed)` | Hash GUID using `hthash_4words` |

### Type

```c
union guid_fast {
    uint8_t g[16];
    uint32_t d[4];
    int64_t x[2];
};
```

Allows byte, word, or double-word access to the 16-byte GUID.

### Generation Algorithm

1. Get 16 random bytes from `random_bytes_get()`
2. AES-256 ECB encrypt current IV, XOR with random bytes
3. Result becomes both output AND next IV (mutex-protected)
4. Set version 4 bit (`out[7] |= 0x40`)
5. Set variant bit (`out[8] |= 0x80`)

### Key Rotation

`guid_tick()` rotates the AES key and IV periodically. New key and IV derived from fresh random bytes. Protects against state compromise — if the IV is observed, ticking renders it useless.

### Endianness

GUIDs are generated as little-endian regardless of host byte order. The first 2 bytes of the last 8-byte half are big-endian per RFC convention.

### Pitfalls

- Mutex-protected — `guid_generate()` acquires `ctx_lock` for each call. NOT suitable for high-frequency generation paths.
- TODO comment in code: endianess of GUIDs in network packets is flagged as unresolved.

---

## hthash (`hthash.h`)

### Purpose

Fast hash functions for internal hash tables. Two implementations selected at configure time:
- **MurmurHash3** — when `HAVE_HW_MULT` (fast hardware multiplication, default on modern hardware)
- **jhash** — Bob Jenkins' hash, fallback for older/slower hardware without fast multiply

### API

| Function | Signature | Purpose |
|----------|-----------|---------|
| `hthash` | `uint32_t hthash(const void *, size_t, uint32_t)` | Hash byte array with seed |
| `hthash32` | `uint32_t hthash32(const uint32_t *, size_t, uint32_t)` | Hash uint32 array with seed |
| `hthash32_mod` | `uint32_t hthash32_mod(const uint32_t *, size_t, uint32_t, uint32_t)` | Hash uint32 array with custom multiplier |

### Inline Helpers

Header provides inline functions for fixed-word-count inputs (avoids length computation):

| Function | Words | Purpose |
|----------|-------|---------|
| `hthash_1words` | 1 | Hash single uint32 |
| `hthash_2words` | 2 | Hash pair of uint32s |
| `hthash_3words` | 3 | Hash triple of uint32s |
| `hthash_4words` | 4 | Hash quad of uint32s (used by `guid_hash`) |
| `hthash_5words` | 5 | Hash five uint32s |
| `hthash_6words` | 6 | Hash six uint32s |

### Implementation Details

**MurmurHash3** (`HAVE_HW_MULT`):
- Mix: `k *= 0xcc9e2d51; k = rol32(k, 15); k *= 0x1b873593; h ^= k; h = rol32(h, 13); h = h*5 + 0xe6546b64`
- Final: `h ^= h >> 16; h *= 0x85ebca6b; h ^= h >> 13; h *= 0xc2b2ae35; h ^= h >> 16`
- Requires fast 32×32→32 multiplication

**jhash** (no `HAVE_HW_MULT`):
- Mix: `__jhash_mix(a, b, c)` — 3-value reversible mix using subtract/XOR/rotate
- Final: `__jhash_final(a, b, c)` — final 3-value mixing
- Init value: `0x9e3779b9` (golden ratio constant)
- No multiplication required

### hthash Byte Handling

`hthash()` for byte arrays handles alignment intelligently:
- If aligned or unaligned access is safe (`UNALIGNED_OK`), processes as uint32s directly
- Otherwise, handles leading/trailing bytes separately, processes middle as uint32s with `get_unaligned()`

---

## Adler32 (`adler32.*`)

### Purpose

Adler-32 checksum computation. Modified zlib-compatible implementation with per-architecture SIMD-optimized paths. Used by zlib internally, and by `ansi_prng.c` to track intermediate PRNG state.

### Architecture

Dispatches like AES: `adler32.c` includes the appropriate `*/adler32.c` based on architecture. Supported paths: `x86/`, `sparc/`, `ppc/`, `alpha/`, `arm/`, `mips/`, `ia64/`, `tile/`, `generic/`.

### API

Standard zlib `adler32()` signature. No project-specific wrapper.

### Usage

- **Primary consumer**: zlib inflate/deflate (via `zalloc`/`zfree` palloc integration)
- **`ansi_prng.c`**: Tracks intermediate PRNG values; `adler ⊕ timestamp` drives counter increment

---

## Cross-Module Relationships

```
ansi_prng ──uses──► aes (bit perturbation)
ansi_prng ──uses──► adler32 (counter perturbation)
guid ──uses──► ansi_prng (random bytes)
guid ──uses──► aes (mixing with IV)
guid ──uses──► hthash (hash function: guid_hash)
```

The AES module is the foundational mixing primitive. Everything else builds on it.
