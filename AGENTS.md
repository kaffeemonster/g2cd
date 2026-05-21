<!-- gitnexus:start -->
# GitNexus — Code Intelligence

This project is indexed by GitNexus as **g2cd**. Use the GitNexus MCP tools to understand code, assess impact, and navigate safely.

> If any GitNexus tool warns the index is stale, run `npx gitnexus analyze --skip-agents-md` in terminal first.

## Always Do
- **MUST run impact analysis before editing any symbol.** Use `gitnexus_impact({target: "symbolName", direction: "upstream"})`.
- **MUST run `gitnexus_detect_changes()` before committing** to verify affected scope.
- **MUST warn the user** if impact analysis returns HIGH or CRITICAL risk before proceeding.
- Use `gitnexus_query({query: "concept"})` for execution flows instead of grepping.
- Use `gitnexus_context({name: "symbolName"})` for full symbol context.

## Never Do
- NEVER edit a function/class/method without running `gitnexus_impact`.
- NEVER ignore HIGH or CRITICAL risk warnings.
- NEVER rename symbols with find-and-replace — use `gitnexus_rename`.
- NEVER commit changes without running `gitnexus_detect_changes()`.

## Resources
| Resource | Use for |
|----------|---------|
| `gitnexus://repo/g2cd/context` | Codebase overview, check index freshness |
| `gitnexus://repo/g2cd/clusters` | All functional areas |
| `gitnexus://repo/g2cd/processes` | All execution flows |
| `gitnexus://repo/g2cd/process/{name}` | Step-by-step execution trace |

## CLI
| Task | Read this skill file |
|------|---------------------|
| Understand architecture / "How does X work?" | `.agents/skills/gitnexus/gitnexus-exploring/SKILL.md` |
| Blast radius / "What breaks if I change X?" | `.agents/skills/gitnexus/gitnexus-impact-analysis/SKILL.md` |
| Trace bugs / "Why is X failing?" | `.agents/skills/gitnexus/gitnexus-debugging/SKILL.md` |
| Rename / extract / split / refactor | `.agents/skills/gitnexus/gitnexus-refactoring/SKILL.md` |
| Tools, resources, schema reference | `.agents/skills/gitnexus/gitnexus-guide/SKILL.md` |
| Index, status, clean, wiki CLI commands | `.agents/skills/gitnexus/gitnexus-cli/SKILL.md` |

<!-- gitnexus:end -->

# CRITICAL RULES - MUST FOLLOW

## HARNESS
- USE LSP, MCP, integrated tool before using the shell

## RESPONSES
- keep responses concise and to the point - unless the user asks otherwise

## PLANING MODE
- Always ask clarifying questions (ask tool)
- Bever asume design, tech stack or features
- Use deep-dive sub-agents to assist with research
- Use deep-dive sub-agents to review the different aspects of your plan before presenting it zo the user

## CHANGE / EDIT MODE
- never implement features youself when possible - use sub-agents
- Identify changes from the plan that can be implemented in parallel, and use sub-agents to implement the features efficiently
- When using sub-agents to implement features, act as a coordinator only
- Use the best model for the task - premium models for complex task (like coding, reasening) and mid-tier models for simpler tasks

## TESTING
- Use any testing tools, libraries available to the project for testing your changes
- Never assume your changes simply work, always test!
- If Project does not have any testing tools, scripts, MCP tools, skills, etc. available for testing, ask the user wheter testing should be skipped.

# Build and Development
-- **Build Process:** `./configure` $\rightarrow$ `make`.
-- **Release Build:** `./configure --enable-release` $\rightarrow$ `make`.
-- **Install:** `make install`.
-- **Run:** `./g2cd`.
-- **Developer Tools:**
-- `make todo`: Extracts `// TODO:` comments from source into `TODO` file. Contains many long-standing observations, open questions, and speculative notes — not all entries are actionable. Review for actionability before pursuing.
-- `make callgraph`: Generate Graphviz callgraph.
-- **Clean:** `make clean` or `make distclean`.
--** **Distribute:** `make dist`
-- **New files/directories:** Any new file (code, docs, headers, etc) needed in a src distribution MUST be manually added to the distribution tarball via its module's `module.make` — add to `TARED_FILES`, `HEADS`, `LIBHEADS`, `SRCS`, `LIBSRCS`, `LIBDOCS`, `AUX`, etc. as appropriate. Directories go in `TARED_DIRS`, mind the directorie order.

# Coding Style and Conventions
- **Indentation:** Use **real tabs** (not spaces). Visual align with spaces
- **Braces:** Opening brace on a new line for blocks; same line for short blocks (2-3 lines).
- **One-Liners:** Place the statement on a new line after the keyword.
- **Naming:** Use `snake_case` for functions and variables; no `CamelCase`.
- **Comments:** /\* \*/ comments
- **TODOs:** Use `// TODO: ` starting at **column 1** on its own line to put in `make todo`.
- **Types:** Use fixed-width types (`_t`) and avoid presumptions about `int` size.
- **C Standard:** C99 with GNU extensions.
- **C preprocessor** directives are indented, # has to stay on first coloumn, 1 space per level until keyword

# Performance Constraint: C10k with Heavy State
- Use `.agents/references/PERFORMANCE.md` for detailed guidiance
- **Scalability:** Solutions must not scale state linearly or worse without resource consideration.
- **Metric:** Performance must remain stable under 10k connections.

# Security Constraint:
- **Network Facing:** Assume all incoming data is hostile.
- Use `.agents/references/SECURITY.md` for detailed guidiance

# Hazard Pointer
- **DO NOT MODIFY:** `hzp_scan()` function in `lib/hzp.c` is highly optimized lock-free logic.
- **Usage:** Agents should **only** use the `hzp_ref`/`hzp_unref`/`hzp_deferfree` API.
    - Never access hazard pointer protected shared data without `hzp_ref()`/`hzp_unref()`.
    - Use `lib/reference/HZP_REFERENCE.md` for correct usage patterns.

# Atomic Operations
- **Custom pre-C11 library** — do NOT use `<stdatomic.h>` or other platform atomics.
- **Only use cross-platform portable API** (see `lib/reference/ATOMIC_REFERENCE.md`).
- **Key types:** `atomic_t`, `atomicptr_t`, `atomicst_t` (stack nodes), `atomicptra_t`/`atomica_t` (padded for arrays on LL/SC archs).
- **`atomic_pop` is non-deterministic under contention** — it may return any top-level element, not strictly LIFO.
- **`atomic_cmpx`/`atomic_cmppx` return the old value** — check success by comparing return against expected old value.
- **Pair with HZP** for safe memory reclamation of shared pointers.

# Compiler Helpers
- **`lib/other.h` is the compiler compatibility layer** — all GCC attributes, barriers, and inline control go through it.
- **Use `GCC_ATTR_*` macros, never raw `__attribute__`** — they degrade to no-ops on older compilers.
- `likely()`/`unlikely()` for branch prediction hints is available**, use in CAS loops and hot paths, or to mark unlikely error paths.
- **Use `always_inline`/`noinline` for inline control** — never write `inline __attribute__((always_inline))` directly.
- **See `lib/reference/OTHER_REFERENCE.md`** for full macro reference, version gates, and fallback behavior.

# Endianness and Unaligned Access
- **NEVER dereference unaligned pointers directly** (e.g., `*(uint32_t*)ptr`) — use `get_unaligned()` or `get_unaligned_endian()`.
- **Use project endian API** (`be32_to_cpu()`, `cpu_to_be32()`, etc.), NOT `ntohl()`/`htonl()` or `<arpa/inet.h>`.
- **`__swab*()` is raw byte reversal** — it swaps unconditionally. Use `cpu_to_*()` / `*_to_cpu()` for semantic host↔endian conversion.
- **`__swab*p()` assumes natural alignment** — for unaligned pointers, use `get_unaligned_be*/le*()` instead.
- **See `lib/reference/ENDIAN_UNALIGNED_REFERENCE.md`** for full API reference and pitfalls.
