#pragma once

#include "storage/file_manager.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wdb::index {

// B+ tree mapping string keys to string values. Nodes live in `storage::Page`s
// allocated through the supplied FileManager. Each node occupies one page.
//
// The tree is rooted at a single page id which the caller is responsible for
// persisting (Database stores it in the meta page). After insert(), check
// root_id(); if it changed the root was split and the caller must persist.
//
// Phase 3 deliberately omits node merging on delete — entries are removed from
// leaves but underfull leaves are not coalesced.
class BTree {
public:
  using PageId = storage::Page::PageId;

  // root_id == 0 means "no tree yet" — a fresh empty leaf is allocated and
  // becomes the root.
  BTree(storage::FileManager &fm, PageId root_id);

  PageId root_id() const { return root_; }

  // Returns true if a new key was inserted, false if an existing key was
  // replaced.
  bool insert(std::string_view key, std::string_view value);

  std::optional<std::string> get(std::string_view key) const;

  // Returns true if the key was present and removed.
  bool remove(std::string_view key);

  // Half-open range scan: emits pairs with from <= k < to, in key order.
  std::vector<std::pair<std::string, std::string>> range(std::string_view from,
                                                         std::string_view to) const;

private:
  storage::FileManager &fm_;
  PageId root_;
};

} // namespace wdb::index
