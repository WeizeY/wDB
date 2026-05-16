# wDB

Single-process, disk-backed database engine built from scratch in C++20.

Key-value storage → WAL → B+ tree index → buffer pool → optional SQL layer.

See [`docs/DESIGN.md`](docs/DESIGN.md) for full architecture and build order.

**Current status:** Phase 1 complete — pages, file manager, heap storage, REPL.

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

## Phase 1 Notes

- **Page size:** 4096 bytes
- **File layout:** page 0 = meta (magic, version, num_pages); pages 1..N = data
- **Record format:** `[flags(1) | key_size(2) | value_size(4) | key | value]`
- **Storage:** linear scan over all data pages (no index yet)
- **Update semantics:** put(k,v) tombstones any prior live entry for k, then appends
- **Checksum:** FNV-1a over the whole page (excluding the checksum field), verified on every read
- **Not yet crash-safe:** no WAL — that's Phase 2

---

## Directory Structure

```
src/
  storage/      Phase 1 ✅ — pages, file_manager, heap_storage, records
  wal/          Phase 2 — write-ahead log
  index/        Phase 3 — B+ tree
  buffer/       Phase 4 — buffer pool / page cache
  query/        Phase 5 — parser + executor
  concurrency/  Phase 6 — locks, thread pool
  db/           Top-level Database facade
docs/
  DESIGN.md     Full design document
tests/          Unit, crash, fuzz tests (to be filled in Phase 8)
Makefile        Thin wrapper around cmake (see "Build & Run")
```
