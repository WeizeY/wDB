#pragma once

#include "storage/file_manager.h"
#include "storage/heap_storage.h"
#include "wal/wal.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace wdb {

// Top-level facade. Phase 2: file_manager + heap_storage + WAL.
// Open path: data file at `path`, WAL at `path + ".wal"`.
// On construction, WAL is replayed into heap before returning.
class Database {
public:
    explicit Database(const std::string& path);

    void put(std::string_view key, std::string_view value);
    std::optional<std::string> get(std::string_view key) const;
    bool del(std::string_view key);

    // Diagnostics.
    size_t num_pages() const { return fm_.num_pages(); }
    size_t num_data_pages() const { return heap_.num_data_pages(); }
    uint64_t wal_size() const { return wal_.size_bytes(); }

private:
    storage::FileManager fm_;
    storage::HeapStorage heap_;
    wal::Wal wal_;
};

}  // namespace wdb
