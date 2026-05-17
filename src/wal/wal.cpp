#include "wal/wal.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>

namespace wdb::wal {

namespace {

constexpr size_t kPayloadHeaderSize = 1 + 2 + 4; // op + key_size + value_size

// Table-based CRC32 (IEEE polynomial, reflected). Computed at static-init time.
const std::array<uint32_t, 256> &crc_table() {
  static const auto table = [] {
    std::array<uint32_t, 256> t{};
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int j = 0; j < 8; ++j) {
        c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      }
      t[i] = c;
    }
    return t;
  }();
  return table;
}

uint32_t crc32(const void *data, size_t n) {
  const auto &table = crc_table();
  uint32_t c = 0xFFFFFFFFu;
  const uint8_t *p = static_cast<const uint8_t *>(data);
  for (size_t i = 0; i < n; ++i) {
    c = table[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFFu;
}

// Read exactly n bytes at offset. Returns bytes actually read (may be < n on
// short read / EOF). Throws on hard I/O error.
ssize_t pread_full(int fd, void *buf, size_t n, off_t off) {
  size_t total = 0;
  auto *p = static_cast<uint8_t *>(buf);
  while (total < n) {
    ssize_t r =
        ::pread(fd, p + total, n - total, off + static_cast<off_t>(total));
    if (r == 0)
      break; // EOF
    if (r < 0) {
      if (errno == EINTR)
        continue;
      throw std::system_error(errno, std::generic_category(), "pread");
    }
    total += static_cast<size_t>(r);
  }
  return static_cast<ssize_t>(total);
}

} // namespace

Wal::Wal(const std::string &path) : fd_(-1), valid_end_(0), replayed_(false) {
  fd_ = ::open(path.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);
  if (fd_ < 0) {
    throw std::system_error(errno, std::generic_category(), "open " + path);
  }
}

Wal::~Wal() {
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

void Wal::append(OpType op, std::string_view key, std::string_view value) {
  if (key.size() > UINT16_MAX) {
    throw std::runtime_error("WAL: key too large");
  }
  if (value.size() > UINT32_MAX) {
    throw std::runtime_error("WAL: value too large");
  }

  const uint16_t ksz = static_cast<uint16_t>(key.size());
  const uint32_t vsz = static_cast<uint32_t>(value.size());
  const uint32_t payload_len =
      static_cast<uint32_t>(kPayloadHeaderSize + ksz + vsz);

  // Build a contiguous frame so the single write() is the unit of atomicity.
  std::vector<uint8_t> frame(4 + payload_len + 4);
  uint8_t *p = frame.data();
  std::memcpy(p, &payload_len, 4);
  p += 4;
  const uint8_t op_byte = static_cast<uint8_t>(op);
  std::memcpy(p, &op_byte, 1);
  p += 1;
  std::memcpy(p, &ksz, 2);
  p += 2;
  std::memcpy(p, &vsz, 4);
  p += 4;
  if (ksz) {
    std::memcpy(p, key.data(), ksz);
    p += ksz;
  }
  if (vsz) {
    std::memcpy(p, value.data(), vsz);
  }

  const uint32_t crc = crc32(frame.data() + 4, payload_len);
  std::memcpy(frame.data() + 4 + payload_len, &crc, 4);

  size_t written = 0;
  while (written < frame.size()) {
    ssize_t w = ::write(fd_, frame.data() + written, frame.size() - written);
    if (w < 0) {
      if (errno == EINTR)
        continue;
      throw std::system_error(errno, std::generic_category(), "wal write");
    }
    written += static_cast<size_t>(w);
  }

  // Bump valid_end_ if we have already replayed (so size_bytes() is
  // meaningful).
  if (replayed_) {
    valid_end_ += frame.size();
  }
}

void Wal::sync() {
  if (::fsync(fd_) < 0) {
    throw std::system_error(errno, std::generic_category(), "wal fsync");
  }
}

std::vector<Record> Wal::replay() {
  std::vector<Record> out;
  off_t off = 0;

  struct stat st {};
  if (::fstat(fd_, &st) < 0) {
    throw std::system_error(errno, std::generic_category(), "fstat wal");
  }
  const off_t end = st.st_size;

  while (off < end) {
    // Read length prefix.
    uint32_t payload_len = 0;
    ssize_t got = pread_full(fd_, &payload_len, 4, off);
    if (got != 4)
      break; // torn length

    const off_t record_total = 4 + static_cast<off_t>(payload_len) + 4;
    if (off + record_total > end)
      break; // torn payload/crc

    // Read payload + trailing crc.
    std::vector<uint8_t> buf(payload_len);
    got = pread_full(fd_, buf.data(), payload_len, off + 4);
    if (got != static_cast<ssize_t>(payload_len))
      break;

    uint32_t stored_crc = 0;
    got = pread_full(fd_, &stored_crc, 4, off + 4 + payload_len);
    if (got != 4)
      break;

    if (crc32(buf.data(), payload_len) != stored_crc)
      break; // corrupt

    // Parse payload.
    if (payload_len < kPayloadHeaderSize)
      break;
    const uint8_t op_byte = buf[0];
    uint16_t ksz = 0;
    uint32_t vsz = 0;
    std::memcpy(&ksz, buf.data() + 1, 2);
    std::memcpy(&vsz, buf.data() + 3, 4);
    if (static_cast<size_t>(kPayloadHeaderSize) + ksz + vsz != payload_len)
      break;
    if (op_byte != static_cast<uint8_t>(OpType::Put) &&
        op_byte != static_cast<uint8_t>(OpType::Del))
      break;

    Record rec;
    rec.op = static_cast<OpType>(op_byte);
    rec.key.assign(
        reinterpret_cast<const char *>(buf.data() + kPayloadHeaderSize), ksz);
    rec.value.assign(
        reinterpret_cast<const char *>(buf.data() + kPayloadHeaderSize + ksz),
        vsz);
    out.push_back(std::move(rec));

    off += record_total;
  }

  valid_end_ = static_cast<uint64_t>(off);
  replayed_ = true;
  return out;
}

void Wal::truncate_to_valid() {
  if (!replayed_) {
    replay(); // populate valid_end_
  }
  if (::ftruncate(fd_, static_cast<off_t>(valid_end_)) < 0) {
    throw std::system_error(errno, std::generic_category(), "ftruncate wal");
  }
}

void Wal::reset() {
  if (::ftruncate(fd_, 0) < 0) {
    throw std::system_error(errno, std::generic_category(), "ftruncate wal");
  }
  valid_end_ = 0;
  replayed_ = true;
}

uint64_t Wal::size_bytes() const {
  struct stat st {};
  if (::fstat(fd_, &st) < 0) {
    throw std::system_error(errno, std::generic_category(), "fstat wal");
  }
  return static_cast<uint64_t>(st.st_size);
}

} // namespace wdb::wal
