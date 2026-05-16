#include "storage/page.h"

#include <gtest/gtest.h>

using namespace wdb::storage;

TEST(Page, InitFreshDataPage) {
    Page p;
    p.init(7, PageType::Data);
    EXPECT_EQ(p.id(), 7u);
    EXPECT_EQ(p.type(), PageType::Data);
    EXPECT_EQ(p.num_records(), 0);
    EXPECT_EQ(p.free_space(), Page::size() - sizeof(PageHeader));
    EXPECT_TRUE(p.records().empty());
}

TEST(Page, AppendThenFind) {
    Page p;
    p.init(1, PageType::Data);
    EXPECT_TRUE(p.append_record("foo", "bar"));
    EXPECT_EQ(p.num_records(), 1);

    auto v = p.find("foo");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, "bar");
    EXPECT_FALSE(p.find("missing").has_value());
}

TEST(Page, AppendShrinksFreeSpace) {
    Page p;
    p.init(2, PageType::Data);
    size_t before = p.free_space();
    ASSERT_TRUE(p.append_record("a", "b"));
    EXPECT_EQ(p.free_space(), before - encoded_record_size(1, 1));
}

TEST(Page, AppendFailsWhenFull) {
    Page p;
    p.init(1, PageType::Data);
    // Fill page with ~32-byte records until it can't take more.
    std::string val(20, 'x');
    int written = 0;
    while (p.append_record("k", val)) {
        ++written;
        if (written > 10000) FAIL() << "infinite append";
    }
    EXPECT_GT(written, 0);
    EXPECT_LT(p.free_space(), encoded_record_size(1, val.size()));
}

TEST(Page, TombstoneHidesFromFind) {
    Page p;
    p.init(3, PageType::Data);
    p.append_record("k", "v");
    ASSERT_TRUE(p.find("k").has_value());

    EXPECT_TRUE(p.tombstone("k"));
    EXPECT_FALSE(p.find("k").has_value());

    // Re-tombstoning live record fails (already gone).
    EXPECT_FALSE(p.tombstone("k"));
}

TEST(Page, RecordsListsAllIncludingTombstoned) {
    Page p;
    p.init(1, PageType::Data);
    p.append_record("a", "1");
    p.append_record("b", "2");
    p.tombstone("a");

    auto recs = p.records();
    ASSERT_EQ(recs.size(), 2u);
    EXPECT_TRUE(recs[0].tombstoned());
    EXPECT_EQ(recs[0].key, "a");
    EXPECT_FALSE(recs[1].tombstoned());
    EXPECT_EQ(recs[1].key, "b");
}

TEST(Page, ChecksumRoundTrip) {
    Page p;
    p.init(5, PageType::Data);
    p.append_record("hello", "world");
    p.update_checksum();
    EXPECT_TRUE(p.verify_checksum());

    // Corrupt one byte in the data region.
    p.data()[sizeof(PageHeader) + 10] ^= 0xAA;
    EXPECT_FALSE(p.verify_checksum());
}

TEST(Page, EmptyValueAllowed) {
    Page p;
    p.init(1, PageType::Data);
    EXPECT_TRUE(p.append_record("k", ""));
    auto v = p.find("k");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, "");
}
