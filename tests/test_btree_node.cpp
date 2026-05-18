#include "index/btree_node.h"

#include "storage/page.h"

#include <gtest/gtest.h>

#include <string>

using namespace wdb::index;
using wdb::storage::Page;
using wdb::storage::PageType;

namespace {

Page make_index_page(Page::PageId id = 1) {
    Page p;
    p.init(id, PageType::Index);
    return p;
}

}  // namespace

TEST(BTreeNode, InitFreshLeaf) {
    Page p = make_index_page();
    BTreeNode node(p);
    node.init_as_leaf();
    EXPECT_TRUE(node.is_leaf());
    EXPECT_FALSE(node.is_internal());
    EXPECT_EQ(node.num_keys(), 0);
    EXPECT_EQ(node.link(), 0u);
    EXPECT_GT(node.free_space(), 4000u);
}

TEST(BTreeNode, InitFreshInternal) {
    Page p = make_index_page();
    BTreeNode node(p);
    node.init_as_internal();
    EXPECT_TRUE(node.is_internal());
    EXPECT_EQ(node.num_keys(), 0);
}

TEST(BTreeNode, LeafInsertAndFind) {
    Page p = make_index_page();
    BTreeNode node(p);
    node.init_as_leaf();

    EXPECT_TRUE(node.leaf_insert("banana", "yellow"));
    EXPECT_TRUE(node.leaf_insert("apple", "red"));
    EXPECT_TRUE(node.leaf_insert("cherry", "red"));
    EXPECT_EQ(node.num_keys(), 3);

    // Keys must be in sorted order.
    EXPECT_EQ(node.leaf_entry(0).key, "apple");
    EXPECT_EQ(node.leaf_entry(1).key, "banana");
    EXPECT_EQ(node.leaf_entry(2).key, "cherry");

    auto v = node.leaf_find("banana");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, "yellow");

    EXPECT_FALSE(node.leaf_find("missing").has_value());
}

TEST(BTreeNode, LeafInsertReplaces) {
    Page p = make_index_page();
    BTreeNode node(p);
    node.init_as_leaf();

    node.leaf_insert("k", "v1");
    node.leaf_insert("k", "v2");
    EXPECT_EQ(node.num_keys(), 1);
    EXPECT_EQ(node.leaf_find("k").value_or(""), "v2");
}

TEST(BTreeNode, LeafRemove) {
    Page p = make_index_page();
    BTreeNode node(p);
    node.init_as_leaf();
    node.leaf_insert("a", "1");
    node.leaf_insert("b", "2");
    node.leaf_insert("c", "3");

    EXPECT_TRUE(node.leaf_remove("b"));
    EXPECT_EQ(node.num_keys(), 2);
    EXPECT_FALSE(node.leaf_find("b").has_value());
    EXPECT_EQ(node.leaf_find("a").value_or(""), "1");
    EXPECT_EQ(node.leaf_find("c").value_or(""), "3");

    EXPECT_FALSE(node.leaf_remove("b"));  // already gone
}

TEST(BTreeNode, LeafInsertFailsWhenFull) {
    Page p = make_index_page();
    BTreeNode node(p);
    node.init_as_leaf();

    // Fill with large values until rejected.
    std::string val(120, 'x');
    int written = 0;
    while (true) {
        std::string key = "k" + std::to_string(written);
        if (!node.leaf_insert(key, val)) break;
        ++written;
        if (written > 1000) FAIL() << "infinite insert";
    }
    EXPECT_GT(written, 0);
}

TEST(BTreeNode, LeafSplit) {
    Page left_page = make_index_page(1);
    Page right_page = make_index_page(2);
    BTreeNode left(left_page);
    BTreeNode right(right_page);
    left.init_as_leaf();
    right.init_as_leaf();

    for (int i = 0; i < 20; ++i) {
        char k[8];
        std::snprintf(k, sizeof(k), "k%03d", i);
        ASSERT_TRUE(left.leaf_insert(k, "value_" + std::to_string(i)));
    }

    std::string sep = left.leaf_split_into(right);
    EXPECT_FALSE(sep.empty());
    EXPECT_EQ(left.num_keys() + right.num_keys(), 20);
    EXPECT_EQ(left.link(), 2u);
    EXPECT_EQ(right.link(), 0u);
    EXPECT_EQ(right.leaf_entry(0).key, sep);
    // Keys in left are all < sep
    for (uint16_t i = 0; i < left.num_keys(); ++i) {
        EXPECT_LT(left.leaf_entry(i).key, sep);
    }
}

TEST(BTreeNode, InternalInsertAndFindChild) {
    Page p = make_index_page();
    BTreeNode node(p);
    node.init_as_internal();
    node.set_link(100);  // leftmost

    EXPECT_TRUE(node.internal_insert("m", 200));  // keys >= "m" → 200
    EXPECT_TRUE(node.internal_insert("t", 300));  // keys >= "t" → 300

    // Search routing:
    EXPECT_EQ(node.internal_find_child("a"), 100u);   // < "m" → leftmost
    EXPECT_EQ(node.internal_find_child("m"), 200u);   // = "m" → first child after
    EXPECT_EQ(node.internal_find_child("p"), 200u);   // [m,t) → 200
    EXPECT_EQ(node.internal_find_child("t"), 300u);   // = "t" → 300
    EXPECT_EQ(node.internal_find_child("z"), 300u);   // > "t" → 300
}

TEST(BTreeNode, InternalSplit) {
    Page left_page = make_index_page(1);
    Page right_page = make_index_page(2);
    BTreeNode left(left_page);
    BTreeNode right(right_page);
    left.init_as_internal();
    right.init_as_internal();
    left.set_link(100);

    for (int i = 0; i < 20; ++i) {
        char k[8];
        std::snprintf(k, sizeof(k), "k%03d", i);
        ASSERT_TRUE(left.internal_insert(k, static_cast<uint32_t>(200 + i)));
    }

    std::string sep = left.internal_split_into(right);
    EXPECT_FALSE(sep.empty());
    // Median is removed from both halves.
    EXPECT_EQ(left.num_keys() + right.num_keys(), 19);
    // All keys in left < sep, all keys in right > sep.
    for (uint16_t i = 0; i < left.num_keys(); ++i) {
        EXPECT_LT(left.internal_entry(i).key, sep);
    }
    for (uint16_t i = 0; i < right.num_keys(); ++i) {
        EXPECT_GT(right.internal_entry(i).key, sep);
    }
}
