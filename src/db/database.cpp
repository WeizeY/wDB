#include "db/database.h"

namespace wdb {

Database::Database(const std::string &path)
    : fm_(path), heap_(fm_), wal_(path + ".wal") {
  // Replay any WAL records into the heap. Idempotent: put() tombstones
  // any prior live entry for the key before appending, and del() of an
  // already-tombstoned key is a no-op.
  const auto records = wal_.replay();
  for (const auto &rec : records) {
    if (rec.op == wal::OpType::Put) {
      heap_.put(rec.key, rec.value);
    } else {
      heap_.del(rec.key);
    }
  }
  // Checkpoint: durable-flush the heap, then reset the WAL.
  // Order matters — heap must be on disk before we drop the log that
  // describes it. If we crash between fm_.sync() and wal_.reset(), the
  // WAL is replayed again on next open; replay is idempotent so this is
  // safe.
  if (!records.empty()) {
    fm_.sync();
    wal_.reset();
  } else {
    // Empty replay still trims any torn tail.
    wal_.truncate_to_valid();
  }
}

void Database::put(std::string_view k, std::string_view v) {
  wal_.append(wal::OpType::Put, k, v);
  wal_.sync();
  heap_.put(k, v);
}

std::optional<std::string> Database::get(std::string_view k) const {
  return heap_.get(k);
}

bool Database::del(std::string_view k) {
  wal_.append(wal::OpType::Del, k);
  wal_.sync();
  return heap_.del(k);
}

} // namespace wdb
