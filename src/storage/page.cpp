#include "storage/page.h"

#include <cstddef>
#include <cstring>
#include <limits>

namespace wdb::storage {

Page::Page() = default;

void Page::init(PageId id, PageType type) {
  buf_.fill(0);
  PageHeader h{};
  h.page_id = id;
  h.page_type = type;
  h.num_records = 0;
  h.free_space_offset = static_cast<uint16_t>(sizeof(PageHeader));
  h.reserved = 0;
  h.checksum = 0;
  h.lsn = 0;
  set_header(h);
}

PageHeader Page::header() const {
  PageHeader h{};
  std::memcpy(&h, buf_.data(), sizeof(PageHeader));
  return h;
}

void Page::set_header(const PageHeader &h) {
  std::memcpy(buf_.data(), &h, sizeof(PageHeader));
}

size_t Page::free_space() const {
  return kPageSize - header().free_space_offset;
}

bool Page::append_record(std::string_view key, std::string_view value) {
  if (key.size() > std::numeric_limits<uint16_t>::max())
    return false;
  if (value.size() > std::numeric_limits<uint32_t>::max())
    return false;

  const size_t need = encoded_record_size(key.size(), value.size());
  if (need > free_space())
    return false;

  PageHeader h = header();
  uint8_t *p = buf_.data() + h.free_space_offset;

  const uint8_t flags = 0;
  const uint16_t ksz = static_cast<uint16_t>(key.size());
  const uint32_t vsz = static_cast<uint32_t>(value.size());

  std::memcpy(p, &flags, 1);
  p += 1;
  std::memcpy(p, &ksz, 2);
  p += 2;
  std::memcpy(p, &vsz, 4);
  p += 4;
  std::memcpy(p, key.data(), ksz);
  p += ksz;
  std::memcpy(p, value.data(), vsz);

  h.free_space_offset = static_cast<uint16_t>(h.free_space_offset + need);
  h.num_records = static_cast<uint16_t>(h.num_records + 1);
  set_header(h);
  return true;
}

std::vector<RecordView> Page::records() const {
  const PageHeader h = header();
  std::vector<RecordView> out;
  out.reserve(h.num_records);

  size_t off = sizeof(PageHeader);
  // Clamp to kPageSize: header field is uint16 (max 65535) but our buffer is
  // 4096. A corrupt page that slipped past the checksum check (e.g. external
  // tampering) must not let us read past the buffer.
  const size_t end = std::min<size_t>(h.free_space_offset, kPageSize);
  for (uint16_t i = 0; i < h.num_records; ++i) {
    if (off + kRecordHeaderSize > end)
      break;
    const uint8_t *p = buf_.data() + off;
    const uint8_t flags = p[0];
    uint16_t ksz;
    uint32_t vsz;
    std::memcpy(&ksz, p + 1, 2);
    std::memcpy(&vsz, p + 3, 4);
    const size_t rec = kRecordHeaderSize + ksz + vsz;
    if (off + rec > end)
      break;

    RecordView rv;
    rv.flags = flags;
    rv.key = std::string_view(
        reinterpret_cast<const char *>(p + kRecordHeaderSize), ksz);
    rv.value = std::string_view(
        reinterpret_cast<const char *>(p + kRecordHeaderSize + ksz), vsz);
    out.push_back(rv);
    off += rec;
  }
  return out;
}

std::optional<std::string> Page::find(std::string_view key) const {
  for (const auto &r : records()) {
    if (!r.tombstoned() && r.key == key) {
      return std::string(r.value);
    }
  }
  return std::nullopt;
}

bool Page::tombstone(std::string_view key) {
  const PageHeader h = header();
  size_t off = sizeof(PageHeader);
  const size_t end = std::min<size_t>(h.free_space_offset, kPageSize);
  for (uint16_t i = 0; i < h.num_records; ++i) {
    if (off + kRecordHeaderSize > end)
      break;
    uint8_t *p = buf_.data() + off;
    const uint8_t flags = p[0];
    uint16_t ksz;
    uint32_t vsz;
    std::memcpy(&ksz, p + 1, 2);
    std::memcpy(&vsz, p + 3, 4);
    const size_t rec = kRecordHeaderSize + ksz + vsz;
    if (off + rec > end)
      break;

    if (!(flags & kRecordTombstone)) {
      std::string_view k(reinterpret_cast<const char *>(p + kRecordHeaderSize),
                         ksz);
      if (k == key) {
        p[0] = static_cast<uint8_t>(flags | kRecordTombstone);
        return true;
      }
    }
    off += rec;
  }
  return false;
}

uint32_t Page::compute_checksum() const {
  // FNV-1a fold over buffer, skipping the 4-byte checksum field.
  constexpr size_t kCsumOff = offsetof(PageHeader, checksum);
  uint32_t h = 2166136261u;
  auto mix = [&](const uint8_t *d, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      h ^= d[i];
      h *= 16777619u;
    }
  };
  mix(buf_.data(), kCsumOff);
  mix(buf_.data() + kCsumOff + 4, kPageSize - kCsumOff - 4);
  return h;
}

void Page::update_checksum() {
  PageHeader h = header();
  h.checksum = compute_checksum();
  set_header(h);
}

bool Page::verify_checksum() const {
  return header().checksum == compute_checksum();
}

} // namespace wdb::storage
