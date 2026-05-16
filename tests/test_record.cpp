#include "storage/record.h"

#include <gtest/gtest.h>

using namespace wdb::storage;

TEST(Record, EncodedSize) {
    EXPECT_EQ(encoded_record_size(0, 0), kRecordHeaderSize);
    EXPECT_EQ(encoded_record_size(4, 8), kRecordHeaderSize + 12);
    EXPECT_EQ(encoded_record_size(1000, 1000), kRecordHeaderSize + 2000);
}

TEST(Record, TombstoneFlag) {
    RecordView a{0, "k", "v"};
    EXPECT_FALSE(a.tombstoned());

    RecordView b{kRecordTombstone, "k", "v"};
    EXPECT_TRUE(b.tombstoned());

    RecordView c{static_cast<uint8_t>(kRecordTombstone | 0x80), "k", "v"};
    EXPECT_TRUE(c.tombstoned());
}

TEST(Record, HeaderSizeIsByteLayoutContract) {
    // The on-disk layout is [u8 flags][u16 key_size][u32 value_size] = 7 bytes.
    EXPECT_EQ(kRecordHeaderSize, static_cast<size_t>(1 + 2 + 4));
}
