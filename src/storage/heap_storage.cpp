#include "storage/heap_storage.h"

#include "storage/record.h"

#include <stdexcept>

namespace wdb::storage {

HeapStorage::HeapStorage(FileManager& fm) : fm_(fm) {}

void HeapStorage::tombstone_all(std::string_view key) {
    for (Page::PageId id = 1; id < fm_.num_pages(); ++id) {
        Page p;
        fm_.read_page(id, p);
        if (p.type() != PageType::Data) continue;
        if (p.tombstone(key)) {
            fm_.write_page(p);
        }
    }
}

void HeapStorage::put(std::string_view key, std::string_view value) {
    tombstone_all(key);

    const size_t need = encoded_record_size(key.size(), value.size());
    if (need > kPageSize - sizeof(PageHeader)) {
        throw std::runtime_error("record larger than a single page");
    }

    // Try existing data pages.
    for (Page::PageId id = 1; id < fm_.num_pages(); ++id) {
        Page p;
        fm_.read_page(id, p);
        if (p.type() != PageType::Data) continue;
        if (p.free_space() < need) continue;
        if (p.append_record(key, value)) {
            fm_.write_page(p);
            return;
        }
    }

    // No room — allocate a new data page.
    const Page::PageId new_id = fm_.allocate_page(PageType::Data);
    Page p;
    fm_.read_page(new_id, p);
    if (!p.append_record(key, value)) {
        throw std::runtime_error("append_record failed on a fresh page");
    }
    fm_.write_page(p);
}

std::optional<std::string> HeapStorage::get(std::string_view key) const {
    for (Page::PageId id = 1; id < fm_.num_pages(); ++id) {
        Page p;
        fm_.read_page(id, p);
        if (p.type() != PageType::Data) continue;
        if (auto v = p.find(key)) return v;
    }
    return std::nullopt;
}

bool HeapStorage::del(std::string_view key) {
    bool any = false;
    for (Page::PageId id = 1; id < fm_.num_pages(); ++id) {
        Page p;
        fm_.read_page(id, p);
        if (p.type() != PageType::Data) continue;
        if (p.tombstone(key)) {
            fm_.write_page(p);
            any = true;
        }
    }
    return any;
}

size_t HeapStorage::num_data_pages() const {
    size_t cnt = 0;
    for (Page::PageId id = 1; id < fm_.num_pages(); ++id) {
        Page p;
        fm_.read_page(id, p);
        if (p.type() == PageType::Data) ++cnt;
    }
    return cnt;
}

}  // namespace wdb::storage
