#include "storage/file_manager.h"
#include "storage/heap_storage.h"
#include "test_helpers.h"

#include <gtest/gtest.h>

#include <string>

using namespace wdb::storage;
using wdb::test::TempFile;

TEST(HeapStorage, GetMissingReturnsNullopt) {
  TempFile tf;
  FileManager fm(tf.path());
  HeapStorage hs(fm);
  EXPECT_FALSE(hs.get("nope").has_value());
}

TEST(HeapStorage, PutGet) {
  TempFile tf;
  FileManager fm(tf.path());
  HeapStorage hs(fm);
  hs.put("k", "v");
  auto v = hs.get("k");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, "v");
}

TEST(HeapStorage, UpdateOverwrites) {
  TempFile tf;
  FileManager fm(tf.path());
  HeapStorage hs(fm);
  hs.put("k", "v1");
  hs.put("k", "v2");
  auto v = hs.get("k");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, "v2");
}

TEST(HeapStorage, DeleteHides) {
  TempFile tf;
  FileManager fm(tf.path());
  HeapStorage hs(fm);
  hs.put("k", "v");
  EXPECT_TRUE(hs.del("k"));
  EXPECT_FALSE(hs.get("k").has_value());
  // Second delete: nothing live to tombstone.
  EXPECT_FALSE(hs.del("k"));
}

TEST(HeapStorage, DeleteThenInsertReturnsNewValue) {
  TempFile tf;
  FileManager fm(tf.path());
  HeapStorage hs(fm);
  hs.put("k", "v1");
  hs.del("k");
  hs.put("k", "v2");
  auto v = hs.get("k");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, "v2");
}

TEST(HeapStorage, ManyKeysSpillToMultiplePages) {
  TempFile tf;
  FileManager fm(tf.path());
  HeapStorage hs(fm);
  const int N = 300;
  for (int i = 0; i < N; ++i) {
    hs.put("k" + std::to_string(i), "value_padding_" + std::to_string(i));
  }
  EXPECT_GT(hs.num_data_pages(), 1u);
  for (int i = 0; i < N; ++i) {
    auto v = hs.get("k" + std::to_string(i));
    ASSERT_TRUE(v.has_value()) << "missing k" << i;
    EXPECT_EQ(*v, "value_padding_" + std::to_string(i));
  }
}

TEST(HeapStorage, PersistsAcrossReopen) {
  TempFile tf;
  {
    FileManager fm(tf.path());
    HeapStorage hs(fm);
    hs.put("k1", "v1");
    hs.put("k2", "v2");
  }
  {
    FileManager fm(tf.path());
    HeapStorage hs(fm);
    EXPECT_EQ(hs.get("k1").value_or(""), "v1");
    EXPECT_EQ(hs.get("k2").value_or(""), "v2");
  }
}

TEST(HeapStorage, RecordTooLargeThrows) {
  TempFile tf;
  FileManager fm(tf.path());
  HeapStorage hs(fm);
  std::string big(8000, 'x');
  EXPECT_THROW(hs.put("k", big), std::runtime_error);
}
