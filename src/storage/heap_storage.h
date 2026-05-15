#pragma once

#include "storage/file_manager.h"
#include "storage/page.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace wdb::storage {

// Heap file storage on top of FileManager.
// Phase 1: no index, linear scan over all data pages.
// Invariant: at most one live (non-tombstoned) record per key, enforced by put().
class HeapStorage {
public:
    explicit HeapStorage(FileManager& fm);

    // Insert-or-update. Tombstones any existing live record for key first.
    void put(std::string_view key, std::string_view value);

    // Linear scan. Returns value if a live record exists.
    std::optional<std::string> get(std::string_view key) const;

    // Tombstone the live record for key. Returns true if found.
    bool del(std::string_view key);

    // Diagnostics.
    size_t num_data_pages() const;

private:
    FileManager& fm_;

    void tombstone_all(std::string_view key);
};

}  // namespace wdb::storage
