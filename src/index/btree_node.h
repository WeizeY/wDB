#pragma once

#include "storage/page.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace wdb::index {

enum class NodeType : uint8_t {
  Invalid = 0,
  Leaf = 1,
  Internal = 2,
};

// Sits at byte 24 of each Page used as a B+ tree node (right after PageHeader).
//
// Slotted page layout for a node:
//   [PageHeader: 24][NodeHeader: 16][slot array, grows forward]
//        ...free space...
//   [record data, grows backward from byte 4096]
//
// A slot is a 2-byte offset (uint16) into the page where the record bytes live.
// Records are appended at (data_offset - record_size) and never moved; deleting
// a slot just shifts the slot array. Records can therefore leak space inside a
// node; that space is reclaimed on split.
//
// Convention for an internal node with N keys and N+1 children:
//   link    = leftmost child         (keys < k[0])
//   slot[i] = (k[i], c[i+1])         (c[i+1] holds keys in [k[i], k[i+1]))
struct NodeHeader {
  uint8_t node_type;
  uint8_t reserved_a;
  uint16_t num_keys;
  uint16_t data_offset; // start of record region (records below)
  uint16_t reserved_b;
  uint32_t link; // leaf: next-leaf id (0 = none). internal: leftmost child id.
  uint32_t reserved_c;
};
static_assert(sizeof(NodeHeader) == 16);
static_assert(std::is_trivially_copyable_v<NodeHeader>);

inline constexpr size_t kPageHeaderSize = 24;
inline constexpr size_t kNodeBodyStart = kPageHeaderSize + sizeof(NodeHeader);
static_assert(kNodeBodyStart == 40);
inline constexpr size_t kSlotSize = sizeof(uint16_t);

class BTreeNode {
public:
  using PageId = storage::Page::PageId;

  explicit BTreeNode(storage::Page &page);

  storage::Page &page() { return page_; }
  const storage::Page &page() const { return page_; }
  PageId page_id() const { return page_.id(); }

  NodeType type() const;
  bool is_leaf() const { return type() == NodeType::Leaf; }
  bool is_internal() const { return type() == NodeType::Internal; }
  uint16_t num_keys() const;
  PageId link() const;
  void set_link(PageId id);

  // Bytes available for a new (slot + record) pair.
  size_t free_space() const;

  void init_as_leaf();
  void init_as_internal();

  // ---- Leaf ----
  struct LeafEntry {
    std::string_view key;
    std::string_view value;
  };
  LeafEntry leaf_entry(uint16_t i) const;

  // Insert or replace. Returns false if no space.
  bool leaf_insert(std::string_view key, std::string_view value);
  bool leaf_remove(std::string_view key);
  std::optional<std::string> leaf_find(std::string_view key) const;

  // Move the upper half of entries into `right` (must be freshly initialised).
  // Returns the separator key (smallest key now in `right`).
  std::string leaf_split_into(BTreeNode &right);

  // ---- Internal ----
  struct InternalEntry {
    std::string_view key;
    PageId right_child;
  };
  InternalEntry internal_entry(uint16_t i) const;

  bool internal_insert(std::string_view key, PageId right_child);
  PageId internal_find_child(std::string_view key) const;

  // Move the upper half of entries into `right`; the median is removed from
  // both halves and returned as the separator to push up to the parent.
  std::string internal_split_into(BTreeNode &right);

  static size_t leaf_record_size(size_t key_size, size_t value_size) {
    return 2 + 4 + key_size + value_size;
  }
  static size_t internal_record_size(size_t key_size) { return 2 + 4 + key_size; }

private:
  storage::Page &page_;

  NodeHeader read_node_header() const;
  void write_node_header(const NodeHeader &h);

  uint16_t get_slot(uint16_t i) const;
  void set_slot(uint16_t i, uint16_t s);

  uint16_t lower_bound_leaf(std::string_view search_key) const;
  uint16_t lower_bound_internal(std::string_view search_key) const;
};

} // namespace wdb::index
