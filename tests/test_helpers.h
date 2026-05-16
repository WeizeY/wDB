#pragma once

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <string>

namespace wdb::test {

// RAII temp file path. Uses /tmp with a unique name; removes on destruction
// (along with the matching `.wal` sibling if present, to support Database tests).
class TempFile {
public:
    TempFile() {
        static std::atomic<uint64_t> counter{0};
        auto n = counter.fetch_add(1, std::memory_order_relaxed);
        path_ = std::filesystem::temp_directory_path() /
                ("wdb_test_" + std::to_string(::getpid()) + "_" + std::to_string(n) + ".dat");
    }
    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
        std::filesystem::remove(path_.string() + ".wal", ec);
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    const std::string& path() const { return path_str_cached(); }
    std::string wal_path() const { return path_str_cached() + ".wal"; }

private:
    const std::string& path_str_cached() const {
        if (cached_.empty()) cached_ = path_.string();
        return cached_;
    }

    std::filesystem::path path_;
    mutable std::string cached_;
};

}  // namespace wdb::test
