#pragma once

#include "storage/file_manager.h"
#include "storage/heap_storage.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace wdb {

// Top-level facade. Phase 1: file_manager + heap_storage only.
// WAL, buffer pool, indexes, concurrency arrive in later phases.
class Database {
public:
    explicit Database(const std::string& path);

    void put(std::string_view key, std::string_view value);
    std::optional<std::string> get(std::string_view key);
    bool del(std::string_view key);

    // Diagnostics.
    size_t num_pages() const { return fm_.num_pages(); }
    size_t num_data_pages() const { return heap_.num_data_pages(); }

private:
    storage::FileManager fm_;
    storage::HeapStorage heap_;
};

}  // namespace wdb
