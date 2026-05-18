#include "index/btree.h"

#include "storage/file_manager.h"
#include "test_helpers.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <string>
#include <vector>

using wdb::index::BTree;
using wdb::storage::FileManager;
using wdb::test::TempFile;

TEST(BTree, EmptyTreeReturnsNullopt) {
  TempFile tf;
  FileManager fm(tf.path());
  BTree tree(fm, 0);
  EXPECT_FALSE(tree.get("anything").has_value());
}

TEST(BTree, InsertGet) {
  TempFile tf;
  FileManager fm(tf.path());
  BTree tree(fm, 0);
  EXPECT_TRUE(tree.insert("k", "v"));
  EXPECT_EQ(tree.get("k").value_or(""), "v");
}

TEST(BTree, InsertReplaces) {
  TempFile tf;
  FileManager fm(tf.path());
  BTree tree(fm, 0);
  EXPECT_TRUE(tree.insert("k", "v1"));  // new
  EXPECT_FALSE(tree.insert("k", "v2")); // replace
  EXPECT_EQ(tree.get("k").value_or(""), "v2");
}

TEST(BTree, ManyInsertsForceSplitsAndRetrieve) {
  TempFile tf;
  FileManager fm(tf.path());
  BTree tree(fm, 0);

  const int N = 500;
  for (int i = 0; i < N; ++i) {
    char k[16];
    std::snprintf(k, sizeof(k), "key_%05d", i);
    ASSERT_TRUE(tree.insert(k, "value_for_" + std::to_string(i)));
  }
  // Tree should have grown beyond a single page → root has changed at least once.
  EXPECT_GT(fm.num_pages(), 3u);

  for (int i = 0; i < N; ++i) {
    char k[16];
    std::snprintf(k, sizeof(k), "key_%05d", i);
    auto v = tree.get(k);
    ASSERT_TRUE(v.has_value()) << "missing " << k;
    EXPECT_EQ(*v, "value_for_" + std::to_string(i));
  }

  EXPECT_FALSE(tree.get("key_99999").has_value());
}

TEST(BTree, RandomOrderInsertsRetrieve) {
  TempFile tf;
  FileManager fm(tf.path());
  BTree tree(fm, 0);

  const int N = 400;
  std::vector<int> order(N);
  for (int i = 0; i < N; ++i)
    order[i] = i;
  std::mt19937 rng(0xC0FFEE);
  std::shuffle(order.begin(), order.end(), rng);

  for (int i : order) {
    char k[16];
    std::snprintf(k, sizeof(k), "k_%05d", i);
    ASSERT_TRUE(tree.insert(k, std::to_string(i)));
  }

  for (int i = 0; i < N; ++i) {
    char k[16];
    std::snprintf(k, sizeof(k), "k_%05d", i);
    auto v = tree.get(k);
    ASSERT_TRUE(v.has_value()) << "missing " << k;
    EXPECT_EQ(*v, std::to_string(i));
  }
}

TEST(BTree, RemoveHidesKey) {
  TempFile tf;
  FileManager fm(tf.path());
  BTree tree(fm, 0);
  tree.insert("a", "1");
  tree.insert("b", "2");
  EXPECT_TRUE(tree.remove("a"));
  EXPECT_FALSE(tree.get("a").has_value());
  EXPECT_EQ(tree.get("b").value_or(""), "2");
  EXPECT_FALSE(tree.remove("a")); // already gone
}

TEST(BTree, RangeScanInOrder) {
  TempFile tf;
  FileManager fm(tf.path());
  BTree tree(fm, 0);
  for (int i = 0; i < 100; ++i) {
    char k[16];
    std::snprintf(k, sizeof(k), "k%03d", i);
    tree.insert(k, std::to_string(i));
  }

  auto results = tree.range("k025", "k035");
  ASSERT_EQ(results.size(), 10u);
  for (size_t i = 0; i < results.size(); ++i) {
    char want[16];
    std::snprintf(want, sizeof(want), "k%03zu", 25 + i);
    EXPECT_EQ(results[i].first, want);
    EXPECT_EQ(results[i].second, std::to_string(25 + i));
  }
}

TEST(BTree, PersistsAcrossReopen) {
  TempFile tf;
  wdb::storage::Page::PageId saved_root = 0;
  {
    FileManager fm(tf.path());
    BTree tree(fm, 0);
    for (int i = 0; i < 200; ++i) {
      char k[16];
      std::snprintf(k, sizeof(k), "k%03d", i);
      tree.insert(k, std::to_string(i));
    }
    fm.set_btree_root(tree.root_id());
    saved_root = tree.root_id();
    fm.sync();
  }
  {
    FileManager fm(tf.path());
    EXPECT_EQ(fm.btree_root(), saved_root);
    BTree tree(fm, fm.btree_root());
    for (int i = 0; i < 200; ++i) {
      char k[16];
      std::snprintf(k, sizeof(k), "k%03d", i);
      auto v = tree.get(k);
      ASSERT_TRUE(v.has_value()) << "missing " << k;
      EXPECT_EQ(*v, std::to_string(i));
    }
  }
}
