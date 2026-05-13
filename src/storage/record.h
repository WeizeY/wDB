#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace wdb::storage {

inline constexpr uint8_t kRecordTombstone = 0x01;

// On-page record layout:
//   [uint8 flags][uint16 key_size][uint32 value_size][key bytes][value bytes]
inline constexpr size_t kRecordHeaderSize = 1 + 2 + 4;

struct RecordView {
    uint8_t flags;
    std::string_view key;
    std::string_view value;

    bool tombstoned() const { return (flags & kRecordTombstone) != 0; }
};

inline size_t encoded_record_size(size_t key_size, size_t value_size) {
    return kRecordHeaderSize + key_size + value_size;
}

}  // namespace wdb::storage
