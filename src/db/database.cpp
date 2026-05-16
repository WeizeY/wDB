#include "db/database.h"

namespace wdb {

Database::Database(const std::string& path)
    : fm_(path), heap_(fm_), wal_(path + ".wal") {
    // Replay any WAL records into the heap. Idempotent: put() tombstones
    // any prior live entry for the key before appending, and del() of an
    // already-tombstoned key is a no-op.
    for (const auto& rec : wal_.replay()) {
        if (rec.op == wal::OpType::Put) {
            heap_.put(rec.key, rec.value);
        } else {
            heap_.del(rec.key);
        }
    }
    // Drop any torn tail we found during replay.
    wal_.truncate_to_valid();
    fm_.sync();
}

void Database::put(std::string_view k, std::string_view v) {
    wal_.append(wal::OpType::Put, k, v);
    wal_.sync();
    heap_.put(k, v);
}

std::optional<std::string> Database::get(std::string_view k) {
    return heap_.get(k);
}

bool Database::del(std::string_view k) {
    wal_.append(wal::OpType::Del, k);
    wal_.sync();
    return heap_.del(k);
}

}  // namespace wdb
