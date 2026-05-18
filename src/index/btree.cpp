#include "index/btree.h"

#include "index/btree_node.h"

#include <stdexcept>

namespace wdb::index {

BTree::BTree(storage::FileManager &fm, PageId root_id) : fm_(fm), root_(root_id) {
  if (root_ == 0) {
    root_ = fm_.allocate_page(storage::PageType::Index);
    storage::Page p;
    fm_.read_page(root_, p);
    BTreeNode node(p);
    node.init_as_leaf();
    fm_.write_page(p);
  }
}

bool BTree::insert(std::string_view key, std::string_view value) {
  // Descend, recording the path so we can propagate splits upward.
  std::vector<PageId> path;
  PageId cur = root_;
  for (;;) {
    storage::Page p;
    fm_.read_page(cur, p);
    BTreeNode node(p);
    path.push_back(cur);
    if (node.is_leaf())
      break;
    cur = node.internal_find_child(key);
  }

  storage::Page leaf_page;
  fm_.read_page(path.back(), leaf_page);
  BTreeNode leaf(leaf_page);
  const bool existed = leaf.leaf_find(key).has_value();

  if (leaf.leaf_insert(key, value)) {
    fm_.write_page(leaf_page);
    return !existed;
  }

  // Leaf is full — split.
  const PageId right_id = fm_.allocate_page(storage::PageType::Index);
  storage::Page right_page;
  fm_.read_page(right_id, right_page);
  BTreeNode right(right_page);
  right.init_as_leaf();

  std::string sep = leaf.leaf_split_into(right);

  if (key < sep) {
    if (!leaf.leaf_insert(key, value)) {
      throw std::runtime_error("leaf_insert after split: still no space (left)");
    }
  } else {
    if (!right.leaf_insert(key, value)) {
      throw std::runtime_error("leaf_insert after split: still no space (right)");
    }
  }

  fm_.write_page(leaf_page);
  fm_.write_page(right_page);

  // Propagate (sep, right_id) up.
  PageId new_child = right_id;
  std::string current_sep = std::move(sep);

  path.pop_back();
  while (!path.empty()) {
    const PageId parent_id = path.back();
    path.pop_back();

    storage::Page parent_page;
    fm_.read_page(parent_id, parent_page);
    BTreeNode parent(parent_page);

    if (parent.internal_insert(current_sep, new_child)) {
      fm_.write_page(parent_page);
      return !existed;
    }

    const PageId parent_right_id = fm_.allocate_page(storage::PageType::Index);
    storage::Page parent_right_page;
    fm_.read_page(parent_right_id, parent_right_page);
    BTreeNode parent_right(parent_right_page);
    parent_right.init_as_internal();

    std::string new_sep = parent.internal_split_into(parent_right);

    if (current_sep < new_sep) {
      if (!parent.internal_insert(current_sep, new_child)) {
        throw std::runtime_error("internal_insert after split: still no space (left)");
      }
    } else {
      if (!parent_right.internal_insert(current_sep, new_child)) {
        throw std::runtime_error("internal_insert after split: still no space (right)");
      }
    }

    fm_.write_page(parent_page);
    fm_.write_page(parent_right_page);

    current_sep = std::move(new_sep);
    new_child = parent_right_id;
  }

  // Root was split — promote a new root.
  const PageId new_root_id = fm_.allocate_page(storage::PageType::Index);
  storage::Page new_root_page;
  fm_.read_page(new_root_id, new_root_page);
  BTreeNode new_root(new_root_page);
  new_root.init_as_internal();
  new_root.set_link(root_); // leftmost = old root (left half)
  if (!new_root.internal_insert(current_sep, new_child)) {
    throw std::runtime_error("internal_insert on fresh root failed");
  }
  fm_.write_page(new_root_page);

  root_ = new_root_id;
  return !existed;
}

std::optional<std::string> BTree::get(std::string_view key) const {
  PageId cur = root_;
  for (;;) {
    storage::Page p;
    fm_.read_page(cur, p);
    BTreeNode node(p);
    if (node.is_leaf())
      return node.leaf_find(key);
    cur = node.internal_find_child(key);
  }
}

bool BTree::remove(std::string_view key) {
  PageId cur = root_;
  for (;;) {
    storage::Page p;
    fm_.read_page(cur, p);
    BTreeNode node(p);
    if (node.is_leaf()) {
      if (!node.leaf_remove(key))
        return false;
      fm_.write_page(p);
      return true;
    }
    cur = node.internal_find_child(key);
  }
}

std::vector<std::pair<std::string, std::string>> BTree::range(std::string_view from,
                                                              std::string_view to) const {
  PageId cur = root_;
  for (;;) {
    storage::Page p;
    fm_.read_page(cur, p);
    BTreeNode node(p);
    if (node.is_leaf())
      break;
    cur = node.internal_find_child(from);
  }

  std::vector<std::pair<std::string, std::string>> out;
  while (cur != 0) {
    storage::Page p;
    fm_.read_page(cur, p);
    BTreeNode node(p);
    const uint16_t n = node.num_keys();
    for (uint16_t i = 0; i < n; ++i) {
      auto e = node.leaf_entry(i);
      if (e.key < from)
        continue;
      if (e.key >= to)
        return out;
      out.emplace_back(std::string(e.key), std::string(e.value));
    }
    cur = node.link();
  }
  return out;
}

} // namespace wdb::index
