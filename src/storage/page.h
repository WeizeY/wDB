#pragma once

#include "storage/record.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace wdb::storage {

inline constexpr size_t kPageSize = 4096;

enum class PageType : uint16_t {
  Invalid = 0,
  Meta = 1,
  Data = 2,
  Index = 3,
};

struct PageHeader {
  uint32_t page_id;
  PageType page_type;
  uint16_t num_records;
  uint16_t
      free_space_offset; // bytes used from page start (where next record goes)
  uint16_t reserved;
  uint32_t checksum;
  uint64_t lsn; // unused in Phase 1, reserved for WAL
};
static_assert(sizeof(PageHeader) == 24, "PageHeader must be 24 bytes");
static_assert(std::is_trivially_copyable_v<PageHeader>,
              "PageHeader must be trivially copyable for memcpy I/O");

class Page {
public:
  using PageId = uint32_t;

  Page();

  // Initialise as a fresh empty page of given id/type.
  void init(PageId id, PageType type);

  // Read the header by value. Safe under strict aliasing — the byte buffer
  // is not punned to PageHeader*; we copy via memcpy.
  PageHeader header() const;

  // Overwrite the header in place.
  void set_header(const PageHeader &h);

  PageId id() const { return header().page_id; }
  PageType type() const { return header().page_type; }
  uint16_t num_records() const { return header().num_records; }

  // Trailing free space in bytes (does not reclaim tombstoned space).
  size_t free_space() const;

  // Append a record. Returns false if not enough space.
  bool append_record(std::string_view key, std::string_view value);

  // Return value of first non-tombstoned record matching key.
  std::optional<std::string> find(std::string_view key) const;

  // Mark first live record matching key as tombstoned. Returns true if found.
  bool tombstone(std::string_view key);

  // Return views of all records (including tombstoned).
  // Views point into this Page's buffer.
  std::vector<RecordView> records() const;

  // Raw buffer access for I/O layer.
  uint8_t *data() { return buf_.data(); }
  const uint8_t *data() const { return buf_.data(); }
  static constexpr size_t size() { return kPageSize; }

  // Checksum covers entire page except the checksum field itself.
  uint32_t compute_checksum() const;
  void update_checksum();
  bool verify_checksum() const;

private:
  alignas(8) std::array<uint8_t, kPageSize> buf_{};
};

} // namespace wdb::storage
