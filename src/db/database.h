#pragma once

#include "index/btree.h"
#include "storage/file_manager.h"
#include "wal/wal.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace wdb {

// Top-level facade. Phase 3: file_manager + B+ tree + WAL.
// Open path: data file at `path`, WAL at `path + ".wal"`.
// On construction, WAL is replayed into the tree before returning.
class Database {
public:
    explicit Database(const std::string& path);

    void put(std::string_view key, std::string_view value);
    std::optional<std::string> get(std::string_view key) const;
    bool del(std::string_view key);

    // Diagnostics.
    size_t num_pages() const { return fm_.num_pages(); }
    uint64_t wal_size() const { return wal_.size_bytes(); }
    index::BTree::PageId btree_root() const { return tree_.root_id(); }

private:
    storage::FileManager fm_;
    index::BTree tree_;
    wal::Wal wal_;

    void sync_root_if_changed();
};

}  // namespace wdb
