<!-- gitnexus:start -->
# GitNexus — Code Intelligence

This project is indexed by GitNexus as **g2cd** (5884 symbols, 12470 relationships, 300 execution flows). Use the GitNexus MCP tools to understand code, assess impact, and navigate safely.

> If any GitNexus tool warns the index is stale, run `npx gitnexus analyze` in terminal first.

## Always Do

- **MUST run impact analysis before editing any symbol.** Before modifying a function, class, or method, run `gitnexus_impact({target: "symbolName", direction: "upstream"})` and report the blast radius (direct callers, affected processes, risk level) to the user.
- **MUST run `gitnexus_detect_changes()` before committing** to verify your changes only affect expected symbols and execution flows.
- **MUST warn the user** if impact analysis returns HIGH or CRITICAL risk before proceeding with edits.
- When exploring unfamiliar code, use `gitnexus_query({query: "concept"})` to find execution flows instead of grepping. It returns process-grouped results ranked by relevance.
- When you need full context on a specific symbol — callers, callees, which execution flows it participates in — use `gitnexus_context({name: "symbolName"})`.

## Never Do

- NEVER edit a function, class, or method without first running `gitnexus_impact` on it.
- NEVER ignore HIGH or CRITICAL risk warnings from impact analysis.
- NEVER rename symbols with find-and-replace — use `gitnexus_rename` which understands the call graph.
- NEVER commit changes without running `gitnexus_detect_changes()` to check affected scope.

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

# Performance Constraint: C10k with Heavy State

## Context
- **Architecture:** Distributed P2P network with Hub/Spoke topology.
- **Role of Hubs:** Elevated nodes maintain persistent state for **all** connected peers.
- **Scale Target:** 10,000+ concurrent connections (C10k baseline).
- **Target Hardware:** General purpose / Legacy / Consumer-grade (Not enterprise server-grade). Do not assume unlimited RAM or PCIe bandwidth.

## Critical Constraint
- **Memory Footprint:**
  - **Per Connection:** State size is approx. ~512 KB (Bloom filter + zlib in/out state + overhead).
  - **Total Hub RAM:** 10k connections × 512 KB ≈ **5 GB**.
  - **Constraint:** The Hub must operate within the total available RAM of the target hardware without thrashing.
- **Primary Bottleneck:** **Memory Bandwidth & CPU Cache Locality** (L1/L2). CPU cycles are secondary to RAM access latency.
- **Secondary Bottleneck:** **System Allocations**. System `malloc`/`free` overhead is unacceptable for high-frequency operations.

## Guidelines for Agents
### 1. Memory Management
- **Zero Allocations:** Do not allocate new objects per request/connection unless strictly necessary.
- **Pools & Slabs:** Use Memory Pools, Slab Allocators, or Thread-Local Storage (TLS) for connection state.
- **Alignment:** Try to avoid unaligned access, a cross cache line access would trigger two cache lines being fetched (a 2 byte read balloning to a **128 bytes fetch** with typicall 64 bytes** cache lines). Certain data may need larger alignment to avoid cache line sharing between threads or for better SIMD access.

### 2. Memory Locality
- **Hot Data:** Avoid random memory access patterns across the connection map.
- **Copy Reduction:** Minimize copying of large blobs. Pass pointers or use zero-copy buffers.

### 3. Concurrency & Safety
- **Model:** Event-driven multiplexing with N worker threads.
- **Thread-Safety:** Critical. Try to use lockless algorithms/structures where possible.
- **Tools:** Project provides Hazard-Pointers implementation. Use them for safe memory reclamation in lock-free structures.

### 4. I/O & Protocol
- **Syscall Model:** Use non-blocking I/O (`epoll`, `io_uring`, or `kqueue`). Blocking I/O is forbidden for main operation.
- **Buffering:** Use fixed-size IO buffers to avoid fragmentation. Reuse buffers across connections.
- **Serialization:** Minimize serialization overhead when propagating state changes. Copying blobs frequently will saturate RAM bandwidth.

### 5. Language Constraints
- **No High-Level Abstractions:** The software is written in C to control machine-level behavior. Avoid dynamic typing patterns or object-oriented inheritance hierarchies that introduce indirection overhead. Avoid overly abstract data access or manipulation patterns.
- **Garbage Collection:** No GC allowed. Manual memory management is required. Be mindful of allocation rates and hidden overhead (e.g., `strdup` in C).
- **Micro-Optimization:** "Death by a thousand cuts." A lot of seemingly small operations add up. Profile before optimizing should be the methodology, but assume every instruction counts from the start.

### 6. Security & Robustness
- **Network Facing:** The application accepts traffic from untrusted networks. Assume all incoming data is hostile.
- **Buffer Safety:** Never trust packet lengths or sizes from the wire. Always validate bounds before `memcpy` or buffer reads/writes. Use safe string functions (`strncpy`, `snprintf`) instead of C-family defaults (`strcpy`, `sprintf`). Use functions of the mem* family in the first place (e.g. `memchr` instead of `strchr`) when the length is known or can be determined. Be mindful of zero-byte injection in external string data.
- **DoS Protection:** Implement rate limiting per IP address/connection.Be mindful of both high-rate floods and low-rate persistent attacks (e.g., Slowloris). A malicious actor should not be able to exhaust the server's resources via connection flooding, state explosion, or resource exhaustion.
- **Integer Safety:** Check for integer overflows before allocation or array indexing. Malformed packets could cause heap corruption or DoS.
- **Endianess:** Be mindfull that protocol data endianess and host cpu endianess might not be the same.
- **Safety-first:** Do not compromise security for performance. Memory safety and validation are paramount.

### 7. Validation
- **Scalability Warning:** Do not suggest solutions that scale state linearly or worse without considering resource limits.
- **Success Metric:** Performance must remain stable under 10k connections. If memory bandwidth is saturated or resources strained, latency will spike.

### 8. Hazard Pointer Scanner (hzp_scan)
- **DO NOT MODIFY:** The `hzp_scan()` function in `lib/hzp.c` is highly optimized lock-free logic.
- **Complexity:** It uses `alloca`, atomic queues, and pointer chasing that is difficult for AI models to verify for correctness.
- **Usage:** Agents should **only** use the `hzp_ref`/`hzp_unref`/`hzp_deferfree` API.
    - Use `lib/HZP_REFERENCE.md` for correct usage patterns.
    - Never access hazard pointer protected shared data without `hzp_ref()`/`hzp_unref()`.
