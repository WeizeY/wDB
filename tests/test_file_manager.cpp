#include "storage/file_manager.h"
#include "storage/page.h"
#include "test_helpers.h"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

using namespace wdb::storage;
using wdb::test::TempFile;

TEST(FileManager, FreshFileCreatesMetaPage) {
    TempFile tf;
    FileManager fm(tf.path());
    EXPECT_EQ(fm.num_pages(), 1u);  // just meta

    Page meta;
    fm.read_page(0, meta);
    EXPECT_EQ(meta.type(), PageType::Meta);
}

TEST(FileManager, AllocateAndReadBack) {
    TempFile tf;
    FileManager fm(tf.path());

    auto id = fm.allocate_page(PageType::Data);
    EXPECT_EQ(id, 1u);
    EXPECT_EQ(fm.num_pages(), 2u);

    Page p;
    fm.read_page(id, p);
    EXPECT_EQ(p.id(), 1u);
    EXPECT_EQ(p.type(), PageType::Data);
}

TEST(FileManager, WriteThenReadPreservesContent) {
    TempFile tf;
    FileManager fm(tf.path());

    auto id = fm.allocate_page(PageType::Data);
    Page p;
    fm.read_page(id, p);
    ASSERT_TRUE(p.append_record("hello", "world"));
    fm.write_page(p);

    Page q;
    fm.read_page(id, q);
    auto v = q.find("hello");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, "world");
}

TEST(FileManager, PersistsAcrossReopen) {
    TempFile tf;
    {
        FileManager fm(tf.path());
        auto id = fm.allocate_page(PageType::Data);
        Page p;
        fm.read_page(id, p);
        ASSERT_TRUE(p.append_record("k", "v"));
        fm.write_page(p);
        fm.sync();
    }
    {
        FileManager fm(tf.path());
        EXPECT_EQ(fm.num_pages(), 2u);
        Page p;
        fm.read_page(1, p);
        auto v = p.find("k");
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, "v");
    }
}

TEST(FileManager, ReadOutOfRangeThrows) {
    TempFile tf;
    FileManager fm(tf.path());
    Page p;
    EXPECT_THROW(fm.read_page(99, p), std::out_of_range);
}

TEST(FileManager, ChecksumMismatchDetectedOnRead) {
    TempFile tf;
    {
        FileManager fm(tf.path());
        auto id = fm.allocate_page(PageType::Data);
        Page p;
        fm.read_page(id, p);
        p.append_record("k", "v");
        fm.write_page(p);
    }

    // Corrupt one byte well inside page 1's payload by writing directly to the file.
    int fd = ::open(tf.path().c_str(), O_RDWR);
    ASSERT_GE(fd, 0);
    uint8_t flip = 0xFF;
    // Page 1 starts at offset kPageSize; corrupt a byte deep in its data region.
    ::pwrite(fd, &flip, 1, kPageSize + 200);
    ::close(fd);

    FileManager fm(tf.path());
    Page p;
    EXPECT_THROW(fm.read_page(1, p), std::runtime_error);
}

TEST(FileManager, NotAWdbFileRejected) {
    TempFile tf;
    // Write garbage that is at least page-sized so the open path tries to read meta.
    {
        int fd = ::open(tf.path().c_str(), O_RDWR | O_CREAT, 0644);
        ASSERT_GE(fd, 0);
        std::string junk(kPageSize, 'Z');
        ::write(fd, junk.data(), junk.size());
        ::close(fd);
    }
    EXPECT_THROW(FileManager fm(tf.path()), std::runtime_error);
}
