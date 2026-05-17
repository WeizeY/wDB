# wDB — Database Engine Design

## Constraints (non-negotiable)

| Building | NOT building |
|---|---|
| Single-process, disk-backed database | Distributed system |
| Key-value storage (initially) | Multi-node replication |
| Optional SQL-like query layer (later) | Full SQL engine from day one |
| Crash recovery | |
| Indexing | |
| Concurrent access (later stage) | |

**Core principle:** Correctness first. Performance second. Extensibility always.

---

## Repository Layout

```
/src
  /storage
    page.h / page.cpp           # Fundamental disk unit
    file_manager.h / file_manager.cpp  # Disk abstraction layer
    record.h / record.cpp       # On-page record format

  /wal
    wal.h / wal.cpp             # Write-ahead log

  /index
    btree.h / btree.cpp         # B+ tree index
    # OR lsm.h / lsm.cpp       # LSM tree (pick one)

  /buffer
    buffer_pool.h / buffer_pool.cpp  # Page cache

  /query
    parser.h / parser.cpp       # Lexer + AST builder
    executor.h / executor.cpp   # AST → index operations

  /concurrency
    locks.h / locks.cpp
    thread_pool.h / thread_pool.cpp

  /db
    database.h / database.cpp   # Top-level facade

  main.cpp
```

Strong modular boundaries from day one. No monolithic `Database.cpp`.

---

## Build Order (strict — do not parallelize early)

1. File manager + pages
2. Heap storage (no index)
3. WAL
4. Recovery
5. Buffer pool
6. B+ tree OR LSM tree
7. Concurrency
8. Testing suite
9. Observability

---

## Phase 1 — Storage Engine

### 1.1 Page Abstraction

Fixed-size page is the fundamental unit of all disk I/O.

```
PAGE_SIZE = 4096 or 8192 bytes
```

**Page structure:**

```
[ PageHeader | FreeSpace | DataRegion ]
```

**PageHeader fields:**

| Field | Type | Notes |
|---|---|---|
| `page_id` | `uint32` | |
| `page_type` | enum | `DATA`, `INDEX`, `META` |
| `free_space_offset` | `uint16` | |
| `number_of_records` | `uint16` | |
| `checksum` | `uint32` | Optional but recommended |

### 1.2 File Manager

Dumb disk API — zero business logic here.

**Responsibilities:**
- Open / close database file
- `read_page(page_id)`
- `write_page(page_id, page)`
- `allocate_page()`

**Implementation notes:**
- Use POSIX `pread` / `pwrite` (not `fstream`)
- Ensure page alignment
- This layer is deliberately "dumb"

### 1.3 Record Format

```
[ key_size | value_size | key | value ]
```

Add later:
- Tombstone flag (deleted marker)
- Version field

**Insert strategy:** append into free space, no compaction yet.

### 1.4 Heap File Storage

First working engine — intentionally inefficient.

```
insert(key, value)
get(key)        ← linear scan through pages
delete(key)     ← tombstone, no compaction
```

### 1.5 Persistence Requirement

At this stage:
- Data written to disk — survives restart
- NOT crash-safe (no WAL yet)

---

## Phase 2 — Write-Ahead Log (WAL)

### Known limitations of the Phase 2 design

These are accepted trade-offs, listed so they aren't rediscovered as bugs:

- **WAL guarantees op durability, not page integrity.** A torn `pwrite` to a heap
  page (process killed mid-write, kernel buffer flushed partially) can leave the
  data page corrupt on disk. The page checksum will detect it on next read, but
  WAL replay alone can't reconstruct the page contents because we don't log
  before/after images — only logical ops. Production fix: double-write buffer
  (InnoDB) or shadow paging (LMDB). Out of scope for Phase 2.

- **WAL grows until clean open.** A long-running process accumulates WAL records
  with no rotation. The Phase 2.4 entry below covers this.

- **fsync ordering on the data file** is best-effort. Pages are flushed only on
  the next clean open (after WAL replay). A crash in normal operation leaves
  pages potentially behind the WAL; recovery brings them forward via replay.


This is where it becomes a real database.

### 2.1 WAL Entry Format

Append-only log file. Each entry:

```
[ op_type | key_size | value_size | key | value | timestamp ]
```

Operations: `INSERT`, `UPDATE`, `DELETE`

### 2.2 Write Path (critical invariant)

**Every mutation:**

```
1. Write to WAL  (fsync optional but preferred)
2. Apply to in-memory / page storage
3. Flush pages eventually
```

This ordering guarantees durability.

### 2.3 Recovery

On startup:

```
1. Open WAL
2. Replay sequentially
3. Reconstruct state in storage engine
```

**Guarantees required:**
- Idempotent replay (safe re-execution)
- Handle partial / corrupt last record (truncation logic)

### 2.4 WAL Management

- WAL rotation when file exceeds size limit
- Checkpointing (later): flush state → truncate WAL

---

## Phase 3 — Indexing Layer

Linear heap scans become unacceptable at scale. Pick **one**:

| | B+ Tree | LSM Tree |
|---|---|---|
| Best for | Classic DB engine, interviews | Modern KV systems |
| Reads | Fast (tree traversal) | Slower (multi-SSTable) |
| Writes | In-place (may split) | Append-only (fast) |
| Complexity | Tree balancing | Compaction logic |

**Recommended default: B+ Tree**

### Option A — B+ Tree

**Node types:**

```
Internal node:  keys[]  +  child_pointers[]
Leaf node:      keys[]  +  values/record_pointers  +  next_leaf_ptr
```

**Disk mapping:** each node = one page (`page_id == node_id`)

**Operations:**

| Op | Path |
|---|---|
| Insert | Find leaf → insert sorted → split on overflow → propagate |
| Search | Root → leaf traversal |
| Delete | Remove entry → rebalance (optional first pass) |

**Why this matters for interviews:** paging logic, tree balancing, disk-aware data structure design.

### Option B — LSM Tree

**Components:**
1. Memtable — in-memory sorted map
2. SSTables — immutable sorted files on disk
3. WAL — already built in Phase 2
4. Compaction system

**Write path:**

```
1. Write WAL
2. Insert into Memtable
3. Flush Memtable → SSTable when full
```

**Read path:**

```
1. Check Memtable
2. Check SSTables (newest → oldest)
3. Optional: Bloom filters to skip SSTables
```

**Compaction:** background merge of SSTables, deduplication, tombstone cleanup.

---

## Phase 4 — Buffer Pool

Page cache layer — reduces disk I/O.

### 4.1 Responsibilities
- Keep hot pages in memory
- Track dirty pages (need flush)

### 4.2 Core Structures
- `HashMap<page_id, frame>`
- LRU list (or CLOCK)

### 4.3 Page Lifecycle

```
pin(page)        ← lock in memory (in use)
unpin(page)      ← release usage counter
mark_dirty(page) ← schedule for flush
```

### 4.4 Eviction

- Start with **LRU** (simple, correct)
- Upgrade to **CLOCK** for more realistic DB behavior

---

## Phase 5 — Query Layer

Do NOT overbuild SQL early.

### 5.1 Minimal API First

```
PUT key value
GET key
DEL key
```

### 5.2 Optional SQL Layer

**Parser:** lexer → tokens → AST

```cpp
// Example AST node
SelectQuery {
    table,
    predicate
}
```

**Executor:** AST → index operations, fallback to scan.

---

## Phase 6 — Concurrency Control

### 6.1 Progression

```
1. Global database mutex         ← start here
2. Per-page locks
3. Reader/writer locks
```

### 6.2 Thread Model
- Thread pool for queries
- Each request executes independently

### 6.3 Hard Problems
- Deadlocks
- Lock ordering
- WAL concurrency
- Page corruption under concurrent writes

---

## Phase 7 — Compaction and Maintenance

| Index type | Work |
|---|---|
| B+ tree | Node merge/split tuning, tree rebalancing |
| LSM tree | Compaction tiers, tombstone cleanup, level management |

---

## Phase 8 — Testing Strategy

### 8.1 Unit Tests
- Page serialization / deserialization
- WAL write and replay correctness
- Index operations (insert, lookup, delete)

### 8.2 Crash Tests
- Simulate `SIGKILL` mid-write
- Partial WAL entry (truncated last record)
- Power-loss equivalent behavior

### 8.3 Fuzz Tests
- Random sequences of insert / delete / read
- Assert consistency after every operation

### 8.4 Benchmarks

| Metric | Tool |
|---|---|
| Ops/sec | Custom harness |
| p95 latency | `perf` / custom timer |
| Cache hit ratio | Internal counters |

---

## Phase 9 — Observability

What separates a toy from an engine:

- Query logs
- Slow query detection
- Internal metrics:
  - Page cache hits / misses
  - WAL file size
  - Cache eviction rate
  - Buffer pool dirty page ratio
