#include "storage/file_manager.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>

namespace wdb::storage {

namespace {

constexpr uint32_t kMetaMagic = 0x77444221; // 'wDB!'
constexpr uint16_t kVersion = 2;            // bumped in Phase 3 to add btree_root

struct MetaPayload {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint32_t num_pages;
  uint32_t btree_root;
};
static_assert(sizeof(MetaPayload) == 16, "MetaPayload must be 16 bytes");

} // namespace

FileManager::FileManager(const std::string &path)
    : fd_(-1), path_(path), num_pages_(0), btree_root_(0) {
  fd_ = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
  if (fd_ < 0) {
    throw std::system_error(errno, std::generic_category(), "open " + path);
  }
  init_or_load_meta();
}

FileManager::~FileManager() {
  if (fd_ >= 0) {
    ::fsync(fd_);
    ::close(fd_);
  }
}

void FileManager::init_or_load_meta() {
  struct stat st {};
  if (::fstat(fd_, &st) < 0) {
    throw std::system_error(errno, std::generic_category(), "fstat");
  }

  if (st.st_size == 0) {
    Page meta;
    meta.init(kMetaPageId, PageType::Meta);
    const MetaPayload mp{kMetaMagic, kVersion, 0, 1, 0};
    std::memcpy(meta.data() + sizeof(PageHeader), &mp, sizeof(mp));
    num_pages_ = 1;
    btree_root_ = 0;
    write_page(meta);
    ::fsync(fd_);
  } else {
    read_meta();
  }
}

void FileManager::read_meta() {
  Page meta;
  const ssize_t n = ::pread(fd_, meta.data(), kPageSize, 0);
  if (n != static_cast<ssize_t>(kPageSize)) {
    throw std::runtime_error("failed to read meta page");
  }
  if (!meta.verify_checksum()) {
    throw std::runtime_error("meta page checksum mismatch");
  }
  if (meta.header().page_type != PageType::Meta) {
    throw std::runtime_error("not a wDB file (bad meta page type)");
  }

  MetaPayload mp;
  std::memcpy(&mp, meta.data() + sizeof(PageHeader), sizeof(mp));
  if (mp.magic != kMetaMagic) {
    throw std::runtime_error("not a wDB file (bad magic)");
  }
  if (mp.version != kVersion) {
    throw std::runtime_error("unsupported wDB version (got " + std::to_string(mp.version) +
                             ", want " + std::to_string(kVersion) + ")");
  }
  num_pages_ = mp.num_pages;
  btree_root_ = mp.btree_root;
}

void FileManager::write_meta() {
  Page meta;
  read_page(kMetaPageId, meta);
  MetaPayload mp;
  std::memcpy(&mp, meta.data() + sizeof(PageHeader), sizeof(mp));
  mp.num_pages = num_pages_;
  mp.btree_root = btree_root_;
  std::memcpy(meta.data() + sizeof(PageHeader), &mp, sizeof(mp));
  write_page(meta);
}

Page::PageId FileManager::allocate_page(PageType type) {
  const Page::PageId id = num_pages_++;
  Page p;
  p.init(id, type);
  write_page(p);
  write_meta();
  return id;
}

void FileManager::set_btree_root(Page::PageId id) {
  btree_root_ = id;
  write_meta();
}

void FileManager::read_page(Page::PageId id, Page &page) const {
  if (id >= num_pages_) {
    throw std::out_of_range("read_page: page_id " + std::to_string(id) + " >= num_pages " +
                            std::to_string(num_pages_));
  }
  const off_t off = static_cast<off_t>(id) * kPageSize;
  const ssize_t n = ::pread(fd_, page.data(), kPageSize, off);
  if (n != static_cast<ssize_t>(kPageSize)) {
    throw std::runtime_error("pread failed for page " + std::to_string(id));
  }
  if (!page.verify_checksum()) {
    throw std::runtime_error("page " + std::to_string(id) + " checksum mismatch");
  }
}

void FileManager::write_page(const Page &page) {
  Page copy = page;
  copy.update_checksum();
  const off_t off = static_cast<off_t>(copy.header().page_id) * kPageSize;
  const ssize_t n = ::pwrite(fd_, copy.data(), kPageSize, off);
  if (n != static_cast<ssize_t>(kPageSize)) {
    throw std::runtime_error("pwrite failed for page " + std::to_string(copy.header().page_id));
  }
}

void FileManager::sync() const {
  if (::fsync(fd_) < 0) {
    throw std::system_error(errno, std::generic_category(), "fsync");
  }
}

} // namespace wdb::storage
