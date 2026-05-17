#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wdb::wal {

enum class OpType : uint8_t {
  Put = 1,
  Del = 2,
};

struct Record {
  OpType op;
  std::string key;
  std::string value; // empty for Del
};

// Append-only WAL file. Phase 2.
//
// On-disk record layout:
//   [uint32 payload_len][payload][uint32 crc32]
// payload layout:
//   [uint8 op][uint16 key_size][uint32 value_size][key bytes][value bytes]
//
// CRC covers payload only. A torn write (short final record or CRC mismatch)
// is detected on replay; truncate_to_valid() can trim it.
class Wal {
public:
  explicit Wal(const std::string &path);
  ~Wal();

  Wal(const Wal &) = delete;
  Wal &operator=(const Wal &) = delete;

  // Append a single record. Does NOT fsync — call sync() to durable-flush.
  void append(OpType op, std::string_view key, std::string_view value = {});

  // fsync the WAL file.
  void sync();

  // Scan from start of file, return all valid records, stop at corruption/EOF.
  std::vector<Record> replay();

  // Trim file to first byte past the last valid record. Useful after replay
  // to discard a torn tail before resuming writes.
  void truncate_to_valid();

  // Reset file to length 0 (drop all WAL contents). Use after successful
  // checkpoint where heap is known durable.
  void reset();

  // Total bytes on disk.
  uint64_t size_bytes() const;

private:
  int fd_;
  uint64_t valid_end_; // updated by replay(); 0 until replay() runs
  bool replayed_;
};

} // namespace wdb::wal
