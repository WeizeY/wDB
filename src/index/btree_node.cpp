#include "index/btree_node.h"

#include <cstring>
#include <stdexcept>

namespace wdb::index {

BTreeNode::BTreeNode(storage::Page &page) : page_(page) {}

NodeHeader BTreeNode::read_node_header() const {
  NodeHeader h{};
  std::memcpy(&h, page_.data() + kPageHeaderSize, sizeof(NodeHeader));
  return h;
}

void BTreeNode::write_node_header(const NodeHeader &h) {
  std::memcpy(page_.data() + kPageHeaderSize, &h, sizeof(NodeHeader));
}

uint16_t BTreeNode::get_slot(uint16_t i) const {
  uint16_t off = 0;
  std::memcpy(&off, page_.data() + kNodeBodyStart + i * kSlotSize, kSlotSize);
  return off;
}

void BTreeNode::set_slot(uint16_t i, uint16_t s) {
  std::memcpy(page_.data() + kNodeBodyStart + i * kSlotSize, &s, kSlotSize);
}

NodeType BTreeNode::type() const { return static_cast<NodeType>(read_node_header().node_type); }

uint16_t BTreeNode::num_keys() const { return read_node_header().num_keys; }

BTreeNode::PageId BTreeNode::link() const { return read_node_header().link; }

void BTreeNode::set_link(PageId id) {
  NodeHeader h = read_node_header();
  h.link = id;
  write_node_header(h);
}

size_t BTreeNode::free_space() const {
  NodeHeader h = read_node_header();
  const size_t slot_end = kNodeBodyStart + h.num_keys * kSlotSize;
  const size_t data_start = h.data_offset;
  if (data_start < slot_end)
    return 0;
  return data_start - slot_end;
}

void BTreeNode::init_as_leaf() {
  NodeHeader h{};
  h.node_type = static_cast<uint8_t>(NodeType::Leaf);
  h.num_keys = 0;
  h.data_offset = static_cast<uint16_t>(storage::kPageSize);
  h.link = 0;
  write_node_header(h);
}

void BTreeNode::init_as_internal() {
  NodeHeader h{};
  h.node_type = static_cast<uint8_t>(NodeType::Internal);
  h.num_keys = 0;
  h.data_offset = static_cast<uint16_t>(storage::kPageSize);
  h.link = 0;
  write_node_header(h);
}

// ---- Leaf ----

BTreeNode::LeafEntry BTreeNode::leaf_entry(uint16_t i) const {
  const uint16_t off = get_slot(i);
  const uint8_t *p = page_.data() + off;
  uint16_t ksz = 0;
  uint32_t vsz = 0;
  std::memcpy(&ksz, p, 2);
  std::memcpy(&vsz, p + 2, 4);
  return {
      std::string_view(reinterpret_cast<const char *>(p + 6), ksz),
      std::string_view(reinterpret_cast<const char *>(p + 6 + ksz), vsz),
  };
}

uint16_t BTreeNode::lower_bound_leaf(std::string_view key) const {
  const uint16_t n = num_keys();
  uint16_t lo = 0, hi = n;
  while (lo < hi) {
    uint16_t mid = static_cast<uint16_t>(lo + (hi - lo) / 2);
    if (leaf_entry(mid).key < key) {
      lo = static_cast<uint16_t>(mid + 1);
    } else {
      hi = mid;
    }
  }
  return lo;
}

bool BTreeNode::leaf_insert(std::string_view key, std::string_view value) {
  const size_t rec_size = leaf_record_size(key.size(), value.size());

  NodeHeader h = read_node_header();
  uint16_t i = lower_bound_leaf(key);

  // Replace path: same key already present — drop its slot first.
  if (i < h.num_keys && leaf_entry(i).key == key) {
    for (uint16_t j = i; j + 1 < h.num_keys; ++j) {
      set_slot(j, get_slot(static_cast<uint16_t>(j + 1)));
    }
    h.num_keys = static_cast<uint16_t>(h.num_keys - 1);
    write_node_header(h);
    // Old record bytes leak (slot no longer references them).
  }

  if (rec_size + kSlotSize > free_space())
    return false;

  h = read_node_header();
  i = lower_bound_leaf(key); // re-find after potential removal

  const uint16_t new_data_offset = static_cast<uint16_t>(h.data_offset - rec_size);
  uint8_t *p = page_.data() + new_data_offset;
  const uint16_t ksz = static_cast<uint16_t>(key.size());
  const uint32_t vsz = static_cast<uint32_t>(value.size());
  std::memcpy(p, &ksz, 2);
  p += 2;
  std::memcpy(p, &vsz, 4);
  p += 4;
  std::memcpy(p, key.data(), ksz);
  p += ksz;
  std::memcpy(p, value.data(), vsz);

  for (int j = h.num_keys; j > i; --j) {
    set_slot(static_cast<uint16_t>(j), get_slot(static_cast<uint16_t>(j - 1)));
  }
  set_slot(i, new_data_offset);

  h.data_offset = new_data_offset;
  h.num_keys = static_cast<uint16_t>(h.num_keys + 1);
  write_node_header(h);
  return true;
}

bool BTreeNode::leaf_remove(std::string_view key) {
  NodeHeader h = read_node_header();
  uint16_t i = lower_bound_leaf(key);
  if (i >= h.num_keys || leaf_entry(i).key != key)
    return false;

  for (uint16_t j = i; j + 1 < h.num_keys; ++j) {
    set_slot(j, get_slot(static_cast<uint16_t>(j + 1)));
  }
  h.num_keys = static_cast<uint16_t>(h.num_keys - 1);
  write_node_header(h);
  return true;
}

std::optional<std::string> BTreeNode::leaf_find(std::string_view key) const {
  uint16_t i = lower_bound_leaf(key);
  if (i >= num_keys())
    return std::nullopt;
  auto e = leaf_entry(i);
  if (e.key != key)
    return std::nullopt;
  return std::string(e.value);
}

std::string BTreeNode::leaf_split_into(BTreeNode &right) {
  NodeHeader h = read_node_header();
  const uint16_t n = h.num_keys;
  const uint16_t split = static_cast<uint16_t>(n / 2);

  for (uint16_t i = split; i < n; ++i) {
    auto e = leaf_entry(i);
    if (!right.leaf_insert(e.key, e.value)) {
      throw std::runtime_error("leaf_split: right child cannot fit upper half");
    }
  }

  h = read_node_header();
  h.num_keys = split;
  write_node_header(h);

  // Chain leaves: right.link = old this.link; this.link = right.id
  NodeHeader rh = right.read_node_header();
  rh.link = h.link;
  right.write_node_header(rh);

  NodeHeader nh = read_node_header();
  nh.link = right.page_id();
  write_node_header(nh);

  return std::string(right.leaf_entry(0).key);
}

// ---- Internal ----

BTreeNode::InternalEntry BTreeNode::internal_entry(uint16_t i) const {
  const uint16_t off = get_slot(i);
  const uint8_t *p = page_.data() + off;
  uint16_t ksz = 0;
  uint32_t child = 0;
  std::memcpy(&ksz, p, 2);
  std::memcpy(&child, p + 2, 4);
  return {
      std::string_view(reinterpret_cast<const char *>(p + 6), ksz),
      child,
  };
}

uint16_t BTreeNode::lower_bound_internal(std::string_view key) const {
  const uint16_t n = num_keys();
  uint16_t lo = 0, hi = n;
  while (lo < hi) {
    uint16_t mid = static_cast<uint16_t>(lo + (hi - lo) / 2);
    if (internal_entry(mid).key < key) {
      lo = static_cast<uint16_t>(mid + 1);
    } else {
      hi = mid;
    }
  }
  return lo;
}

bool BTreeNode::internal_insert(std::string_view key, PageId right_child) {
  const size_t rec_size = internal_record_size(key.size());
  if (rec_size + kSlotSize > free_space())
    return false;

  NodeHeader h = read_node_header();
  uint16_t i = lower_bound_internal(key);

  if (i < h.num_keys && internal_entry(i).key == key) {
    return false; // duplicate separator — caller bug
  }

  const uint16_t new_data_offset = static_cast<uint16_t>(h.data_offset - rec_size);
  uint8_t *p = page_.data() + new_data_offset;
  const uint16_t ksz = static_cast<uint16_t>(key.size());
  const uint32_t child = static_cast<uint32_t>(right_child);
  std::memcpy(p, &ksz, 2);
  p += 2;
  std::memcpy(p, &child, 4);
  p += 4;
  std::memcpy(p, key.data(), ksz);

  for (int j = h.num_keys; j > i; --j) {
    set_slot(static_cast<uint16_t>(j), get_slot(static_cast<uint16_t>(j - 1)));
  }
  set_slot(i, new_data_offset);

  h.data_offset = new_data_offset;
  h.num_keys = static_cast<uint16_t>(h.num_keys + 1);
  write_node_header(h);
  return true;
}

BTreeNode::PageId BTreeNode::internal_find_child(std::string_view key) const {
  PageId child = link(); // leftmost (keys < k[0])
  const uint16_t n = num_keys();
  for (uint16_t i = 0; i < n; ++i) {
    auto e = internal_entry(i);
    if (e.key > key)
      break;
    child = e.right_child;
  }
  return child;
}

std::string BTreeNode::internal_split_into(BTreeNode &right) {
  NodeHeader h = read_node_header();
  const uint16_t n = h.num_keys;
  const uint16_t mid = static_cast<uint16_t>(n / 2);

  auto median = internal_entry(mid);
  std::string sep(median.key);

  // right's leftmost (link) = median's right_child
  NodeHeader rh = right.read_node_header();
  rh.link = median.right_child;
  right.write_node_header(rh);

  for (uint16_t i = static_cast<uint16_t>(mid + 1); i < n; ++i) {
    auto e = internal_entry(i);
    if (!right.internal_insert(e.key, e.right_child)) {
      throw std::runtime_error("internal_split: right cannot fit upper half");
    }
  }

  h = read_node_header();
  h.num_keys = mid;
  write_node_header(h);

  return sep;
}

} // namespace wdb::index
