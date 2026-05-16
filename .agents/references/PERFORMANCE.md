# Performance Constraint: C10k with Heavy State

## Context
- **Architecture:** Distributed P2P network (Hub/Spoke)
- **Role of Hubs:** Elevated nodes maintain persistent state for **all** connected peers (~512kb per peer).
- **Scale Target:** 10,000+ concurrent connections (C10k baseline).
- **Target Hardware:** General purpose / Legacy / Consumer-grade (Not enterprise server-grade).
- **Bottleneck:** Memory bandwidth and CPU cache locality (L1/L2) are the primary bottlenecks. Constant cache trashing due to normaly low traffic connections. No huge number of cores to distribute work.

## Critical Constraint
- **Memory Footprint:**
  - **Per Connection:** State size is approx. ~512 KB (Bloom filter + zlib in/out state + overhead).
  - **Total Hub RAM:** 10k connections × 512 KB ≈ **5 GB** minimum. Not accounting for overhead and System/other processes.
  - **Constraint:** The Hub must operate within the total available RAM and performance of the target hardware without thrashing, protocol processing latency has to be keept reasonable.
- **Primary Bootleneck: Locking**. Overly long held global locks or whole subsytem locks tank overall pkt/s. Beware of hidden contention points (like System allocor). Excessive locking strains Inter CPU Core links.
- **Secondary Bottleneck:** **Memory Bandwidth & CPU Cache Locality** (L1/L2). CPU cycles are secondary to RAM access latency.
- **Tertiary Bottleneck:** **System Allocations**. System `malloc`/`free` overhead is unacceptable for overall high-frequency operations.

## Guidelines for Agents
### 1. Memory Management
- **Zero Allocations:** Do not allocate new objects per request/connection unless strictly necessary.
- **Pools & Slabs:** Use Memory Pools, Slab Allocators, or Thread-Local Storage (TLS) for connection state.
    - TODO: 
Avoid unaligned access to prevent multiple cache line fetches (64-byte lines).
- **Alignment:** Avoid unaligned access, to prevent multiple cache line fetch (64 bytes). Certain data may need larger alignment to avoid cache line sharing between threads or for better SIMD access.
    - TODO: point to reference (TBW) for unaligned helper `lib/unaligned.h`, some architectures ouright can't do unaligned access, so deliberate unaligned access to extract protocol data from buffer has to be filtert through these helper, see also endianess.

### 2. Memory Locality
- **Hot Data:** Avoid random memory access patterns across the connection map.
- **Copy Reduction:** Minimize copying of large and medium blobs. Pass pointers or use zero-copy buffers.

### 3. Concurrency & Safety
- **Model:** Event-driven multiplexing with N worker threads.
- **Thread-Safety:** Critical. Try to use lockless algorithms/structures where possible.
    - TODO: point to reference (TBW) for homegrown atomic ops `lib/atomic.h`
- **Tools:** Project provides Hazard-Pointers implementation. Use them for safe memory reclamation in lock-free structures.

### 4. I/O & Protocol
- **Syscall Model:** Use non-blocking I/O (`epoll`, `io_uring`, or `kqueue`). Blocking I/O is forbidden for main operation.
- **Buffering:** Use fixed-size IO buffers to avoid fragmentation. Reuse buffers across connections.
  - TODO: point to reference (TBW) for `lib/sec_buffer.h`
- **Serialization:** Minimize serialization overhead when propagating state changes. Copying blobs frequently will saturate RAM bandwidth.

### 5. Language Constraints
- **No High-Level Abstractions:** Written in C to control machine-level behavior. Avoid dynamic typing patterns or OO inheritance hierarchies. Be mindfull of over-abstraction overhead. Avoid overly abstract data access or manipulation.
- **Garbage Collection:** No GC allowed due to overhead/unpredictablity. Manual memory management is required. Be mindful of allocation rates.
- **Hidden overhead:** even simple external library functions can have hidden overhead (e.g. `strdup` in C hits the system allocator). In-project library functions are written to avoid this (e.g. do-less paradigm/reduced capability, explicit overhead (e.g. caller has to provide memory), thin abstration wrapper - not full fledged everything-and-the-kitchen-sink (e.g. thin epoll emulation instead of 3rd party event-lib to abstract system differences)).
- **Micro-Optimization:** "Death by a thousand cuts." A lot of seemingly small operations add up. Overgenerilzed lib function can have significant overhead (e.g. `sprintf` to convert a number to text without any need for *very* fancy formating). Profile before optimizing should be the methodology, but assume every instruction counts from the start. Millions of small copies/calls/calculation to process one packet hurt overall packet rate.
    - TODO: point to several optmized lib/ functions? but how, complex topic... difference between doing less (e.g. itoa.h) and doing more (SIMD e.g. memxorcpy, memandn, mempopcnt)

### 6. Security
- **Network Facing:** Server-Process on public untrusted networks. Assume all incoming data is hostile.
- **Safety-first:** Do not compromise security for performance. Memory safety and validation are paramount.
  - for additinal guidiance see `.agents/references/SECURITY.md`

### 7. Robustness
- **Correctness:** Do not compromise Protocol correctness for Performance.
- **Endianess:** Protocol and host endianness may differ; handle explicitly. Protocol can switch endianess, marked per packet.
    - TODO: endian helper in `lib/swab.h`, and the fusion: endian forcing unaligned helper like get_unaligned_be32(), conditinal unaligned helper get_unaligned_endian()

### 8. Validation
- **Scalability:** Solutions must not scale state linearly or worse without resource consideration.
- **Success Metric:** Performance must remain stable under 10k connections.
