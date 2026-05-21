# Build-Time Utilities Reference

## Overview

These files contain `main()` functions but are **not** part of the runtime server. They are invoked during the build process to generate code, data tables, object files, or provide build infrastructure.

## Quick Reference

| Utility | Category | Purpose |
|---------|----------|---------|
| `ccdrv.c` | Build infrastructure | CC driver — pretty-prints build output with curses |
| `arflock.c` | Build infrastructure | Proxy lock for `ar` — enables parallel `make` |
| `G2PacketTyperGenerator.c` | Code generation | Generates binary-search packet type lookup header |
| `G2HeaderFieldsSort.c` | Code generation | Sorts and generates header field lookup tables |
| `bin2o.c` | Code generation | Embeds binary data into object files via assembly |
| `calltree.c` | Analysis | Parses GCC RTL dump, generates Graphviz call graph |
| `bpfasm.py` | Code generation | BPF assembly → C `struct bpf_insn` array converter |
| `lib/five_tab_gen.c` | Data tables | Pre-computes powers of five for float-to-decimal conversion |
| `lib/aes_tab_gen.c` | Data tables | Pre-computes AES lookup tables (T-tables, L-tables) |

---

## ccdrv.c — CC Driver

Pretty-prints build output. Wraps compiler/linker commands and produces clean, colorized console output using `curses`. Idea derived from NcFTP's `ccdv`.

### Usage

```
ccdrv [-v] [-s] [comment] command...
```

### Options

| Option | Description |
|--------|-------------|
| `-v` | Verbose mode — dumps raw command, skips comment display |
| `-s` | Show failed command on error |
| `[comment]` | Optional human-readable comment displayed instead of raw command |
| `command...` | The actual compiler/linker invocation |

### Notes

- Uses `curses`/`term.h` for terminal control with colored progress indicators
- Pipes child stdout/stderr for real-time progress display
- 415 lines

---

## arflock.c — Archive File Lock

Proxy wrapper around `ar` that acquires a file lock on a `.a` archive before invoking `ar`, enabling safe concurrent build operations with parallel `make`.

### Usage

```
arflock <archive.a> <ar-arguments>
```

### Notes

- Two-stage locking: first locks a gate file (`.arflockgate`) when the archive doesn't yet exist, then locks the archive itself
- Uses `lockf()` for POSIX advisory locking with 30-second timeout via `SIGALRM`
- Writes PID to gate file for debugging
- Exits with `ar`'s exit status
- 206 lines

---

## G2PacketTyperGenerator.c — Packet Type Table Generator

Automatically generates `G2PacketTyper.h` — a binary-search lookup table for G2 packet type identification.

### Usage

```
G2PacketTyperGenerator [-v] [-b|-l] [-c] [output-file]
```

### Options

| Option | Description |
|--------|-------------|
| `-v` | Verbose — dumps table entries to stdout |
| `-b` | Target is big-endian |
| `-l` | Target is little-endian |
| `-c` | Print weight statistics and exit (debug mode) |
| `[output-file]` | Output file (default: `G2PacketTyper.h`) |

### Notes

- Reads packet type definitions from `G2Packet.h` (`G2_PACKET_TYPES` macro / `ENUM_CMD` entries)
- Handles 8-byte packet type tags by splitting into two 4-byte keys chained via an index
- Performs byte-swap (`swab32`/`swab64`) when host endianness differs from target
- Uses `qsort` for binary-search-friendly ordering
- 484 lines

---

## G2HeaderFieldsSort.c — Header Field Table Generator

Sorts G2 header field definitions by field name (case-insensitive) and generates `G2HeaderFields.h` for fast runtime header parsing.

### Usage

```
G2HeaderFieldsSort [-|output-file]
```

### Notes

- Single positional arg is the output file; `-` or no arg writes to stdout
- 36 hardcoded `action_string` entries (`h_as00` through `h_as35`)
- Sorts by string length first, then case-insensitive text
- Builds a `max_txt`-sized index mapping string length → first index and count per length for O(1) length-based dispatch
- Includes timestamp comment in output
- 228 lines

---

## bin2o.c — Binary to Object Converter

Converts binary data files (or entire directories) into object files by generating assembly code and piping it through the system assembler (`as`).

### Usage

```
bin2o [options] input-files...
```

### Options

| Option | Description |
|--------|-------------|
| `-o name` | Output object file name |
| `-p name` | Pack multiple inputs into a single symbol |
| `-a name` | Assembler program (default: `as`) |
| `-d {gas\|sun\|coff}` | Assembler dialect: GAS, Sun, COFF |
| `-l N` | Byte alignment (default: 8) |
| `-v` | Verbose |
| `-e` | Export base data symbol |
| `-u` | Unhidden trampolines |
| `-r` | Relative trampolines |
| `-n` | No trampolines |
| `-c` | Use `%` as special character (instead of `@`) |
| `--` | End of options |

### Notes

- Pipes generated assembly to `as` via `fork`/`pipe`
- Supports three assembler dialects
- Uses `mmap` when available
- Each input file gets a `.section .rodata` with `.byte` directives
- Trampoline symbols provide size and pointer access
- 694 lines — the most complex utility

---

## calltree.c — Call Graph Generator

Parses GCC RTL dump files (`-dr` output) and generates a Graphviz call-graph DOT file showing function call relationships.

### Usage

```
calltree [-o omit-list] [-f outfile] [-c] rtl-dump-files...
```

### Options

| Option | Description |
|--------|-------------|
| `-f outfile` | Write output to named file (default: stdout) |
| `-o foo,bar,...` | Comma-separated list of functions to omit |
| `-c` | Enable Graphviz `concentrate` option |
| `rtl-dump-files` | One or more GCC RTL dump files (`-` for stdin) |

### Notes

- Idea from the Perl script "egypt"
- Parses `;; Function NAME` markers and `call symbol_ref(...)` / `call mem:` lines
- Builds hash table of functions, sorts output by source filename
- Generates direct (`->`) and indirect (`-.->`) call edges
- 624 lines

---

## bpfasm.py — BPF Assembler

Converts human-readable BPF (Berkeley Packet Filter) assembly into C `struct bpf_insn[]` arrays.

### Usage

```
bpfasm.py [-o outfile] [-c] [-s] <infile>
```

### Options

| Option | Description |
|--------|-------------|
| `-o` / `--outfile` | Output C file (default: stdout) |
| `-c` / `--const` | Add `const` qualifier to output array |
| `-s` / `--static` | Add `static` qualifier |
| `<infile>` | BPF assembly file (`-` for stdin) |

### Notes

- Written in Python 2.x (backward compatible with <2.4)
- Supports all standard BPF instruction types: LD, LDX, ST, STX, ALU, JMP, RET, MISC
- Supports all address modes: ABS, IND, IMM, MEM, LEN, MSH
- Supports `net[...]` and `ll[...]` offset prefixes for extended BPF
- Resolves forward labels and computes jump displacements
- Output: `static const struct bpf_insn <name>[] = { BPF_STMT(...), BPF_JUMP(...), ... };`
- Beer-Ware License
- 474 lines

---

## lib/five_tab_gen.c — Powers of Five Table Generator

Pre-computes powers of five (`5^1` through `5^325`) as big-integer arrays for use in floating-point-to-decimal conversion routines.

### Usage

```
five_tab_gen <output-file>
```

### Notes

- Self-contained computation, no input required
- Output: binary file containing `MAX_FIVE` (325) entries of `struct big_num`
- Each entry: `BIGSIZE` (24) array of `uint64_t` digits plus a length field
- Implements software big-integer multiplication (`MUL_BIG` macro) with split 32-bit arithmetic
- Iteratively multiplies by 5 starting from `five_tab[0] = {5, len=0}`
- 127 lines — the smallest C utility

---

## lib/aes_tab_gen.c — AES Lookup Table Generator

Pre-computes AES lookup tables (forward/inverse S-box, T-tables, L-tables) for Rijndael encryption/decryption.

### Usage

```
aes_tab_gen <output-file>
```

### Notes

- Output filename must contain one of: `ft_tab`, `fl_tab`, `it_tab`, or `il_tab` to select which table
- Tables computed from Rijndael's GF(2^8) arithmetic with polynomial `0x011b`
- Generates: `aes_ft_tab` (forward T), `aes_fl_tab` (forward L), `aes_it_tab` (inverse T), `aes_il_tab` (inverse L)
- Uses log/pow tables for GF(2^8) multiplication
- Output: binary file with 4 × 256 `uint32_t` values
- Derived from Linux kernel's `aes_generic.c` (itself derived from Brian Gladman's code)
- 217 lines
