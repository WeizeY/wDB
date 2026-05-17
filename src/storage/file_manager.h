#pragma once

#include "storage/page.h"

#include <string>

namespace wdb::storage {

// Owns the database file. Dumb disk API — no business logic.
// File layout: page 0 is meta, pages 1..N-1 are data/index pages.
class FileManager {
public:
  explicit FileManager(const std::string &path);
  ~FileManager();

  FileManager(const FileManager &) = delete;
  FileManager &operator=(const FileManager &) = delete;

  // Allocate a fresh page. Writes the empty page to disk and updates meta.
  Page::PageId allocate_page(PageType type = PageType::Data);

  // Read page `id` into `page`. Throws on bad checksum or I/O error.
  void read_page(Page::PageId id, Page &page) const;

  // Write page to disk. Updates checksum internally.
  void write_page(const Page &page);

  Page::PageId num_pages() const { return num_pages_; }

  void sync() const;

  static constexpr Page::PageId kMetaPageId = 0;

private:
  int fd_;
  std::string path_;
  Page::PageId num_pages_;

  void init_or_load_meta();
  void read_meta();
  void write_meta_num_pages();
};

} // namespace wdb::storage
