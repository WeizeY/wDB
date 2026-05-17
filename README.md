# wDB

Single-process, disk-backed database engine built from scratch in C++20.

Key-value storage → WAL → B+ tree index → buffer pool → optional SQL layer.

See [`docs/DESIGN.md`](docs/DESIGN.md) for full architecture and build order.

**Current status:** Phase 2 complete — pages, file manager, heap storage, REPL, write-ahead log + crash recovery.

---

## Dev Container

```bash
docker compose up -d --build
docker exec -it wdb bash
```

---

## Build & Run (inside container)

The repo includes a thin `Makefile` wrapper around cmake/ninja. All artefacts land in `build/`.

```bash
make build         # configure + compile (Debug, ASan+UBSan)
make run           # build then start REPL on ./wdb.data
make release       # Release build into build-release/
make test          # run ctest
make clean         # delete build/ and build-release/
make fmt           # clang-format src/
```

Manual cmake (equivalent):

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -S .
cmake --build build
./build/src/wdb wdb.data
```

---

## Interacting with wDB (REPL)

```
$ ./build/src/wdb mydb.data
wDB v0.1 — opened mydb.data (pages=1)
Type HELP for commands.
wDB> PUT name alice
OK
wDB> PUT lang c++ with extra words
OK
wDB> GET name
alice
wDB> GET lang
c++ with extra words
wDB> DEL name
OK
wDB> GET name
(nil)
wDB> STAT
pages=2 data_pages=1
wDB> EXIT
```

### Commands

| Command | Effect |
|---|---|
| `PUT <key> <value>` | Insert or update (value may contain spaces) |
| `GET <key>` | Print value, or `(nil)` |
| `DEL <key>` | Tombstone key |
| `STAT` | Show page counts |
| `HELP` | Show commands |
| `EXIT` / `QUIT` | Close DB cleanly |

### Scripted use

Pipe commands over stdin:

```bash
printf 'PUT a 1\nPUT b 2\nGET a\nEXIT\n' | ./build/src/wdb mydb.data
```

The data file persists across runs — close and reopen, your data is still there.

---

## Engine Notes (Phases 1–2)

- **Page size:** 4096 bytes
- **Data file layout:** page 0 = meta (magic, version, num_pages); pages 1..N = data
- **Record format:** `[flags(1) | key_size(2) | value_size(4) | key | value]`
- **Storage:** linear scan over all data pages (no index yet — Phase 3)
- **Update semantics:** put(k,v) tombstones any prior live entry for k, then appends
- **Page checksum:** FNV-1a over the whole page (excluding the checksum field), verified on every read
- **WAL:** append-only log at `<data>.wal`. Frame = `[u32 len][payload][u32 CRC32]`. fsync per mutation
- **Recovery:** WAL replay on open is idempotent. Torn final frame is detected (length-bound or CRC mismatch) and trimmed
- **Checkpoint:** on clean open, after replay + heap fsync, the WAL is reset

### Known limitations (deferred phases)

- **Torn data-page writes** — a crash mid-`pwrite` to a heap page can leave the page corrupt; checksum catches it on next read but recovery from WAL alone won't fix the page. Future fix: double-write buffer or shadow paging
- **WAL has no rotation** — log grows until the next clean open. Phase 2.4
- **No buffer pool** — every read pulls a fresh page from disk. Phase 4
- **No concurrency** — single-thread only. Phase 6

---

## Directory Structure

```
src/
  storage/      Phase 1 — pages, file_manager, heap_storage, records
  wal/          Phase 2 — write-ahead log
  db/           Top-level Database facade
docs/
  DESIGN.md     Full design document
tests/          Unit tests (gtest); crash + fuzz arrive in Phase 8
Makefile        Thin wrapper around cmake (see "Build & Run")
```

Future-phase modules (`index/`, `buffer/`, `query/`, `concurrency/`) will be added
when their phase begins, rather than carried as empty stubs.
