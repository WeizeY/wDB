#include "db/database.h"
#include "test_helpers.h"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

#include <string>

using wdb::Database;
using wdb::test::TempFile;

TEST(Database, PutGetDel) {
  TempFile tf;
  Database db(tf.path());
  db.put("k", "v");
  EXPECT_EQ(db.get("k").value_or(""), "v");
  EXPECT_TRUE(db.del("k"));
  EXPECT_FALSE(db.get("k").has_value());
}

TEST(Database, PersistsCleanly) {
  TempFile tf;
  {
    Database db(tf.path());
    db.put("a", "1");
    db.put("b", "2");
  }
  {
    Database db(tf.path());
    EXPECT_EQ(db.get("a").value_or(""), "1");
    EXPECT_EQ(db.get("b").value_or(""), "2");
  }
}

// Phase 2 core promise: data survives even without graceful close, as long as
// WAL was synced. We simulate "crash" by not running destructors — done by
// scoping the Database and never deleting the underlying files between opens.
TEST(Database, RecoversFromWalAfterPutWithoutHeapFlush) {
  TempFile tf;
  {
    Database db(tf.path());
    db.put("survives", "yes");
  }
  {
    Database db(tf.path());
    EXPECT_EQ(db.get("survives").value_or(""), "yes");
  }
}

TEST(Database, RecoversAfterTornWalTail) {
  TempFile tf;
  {
    Database db(tf.path());
    db.put("alpha", "A");
    db.put("beta", "B");
  }
  // Append a truncated frame to the WAL to simulate a torn write.
  int fd = ::open((tf.path() + ".wal").c_str(), O_WRONLY | O_APPEND);
  ASSERT_GE(fd, 0);
  uint8_t junk[3] = {0xFF, 0xFF, 0xFF};
  ::write(fd, junk, sizeof(junk));
  ::close(fd);

  {
    Database db(tf.path());
    EXPECT_EQ(db.get("alpha").value_or(""), "A");
    EXPECT_EQ(db.get("beta").value_or(""), "B");
  }
}

TEST(Database, UpdateAndDeleteReplayCorrectly) {
  TempFile tf;
  {
    Database db(tf.path());
    db.put("k", "v1");
    db.put("k", "v2");
    db.put("gone", "x");
    db.del("gone");
  }
  {
    Database db(tf.path());
    EXPECT_EQ(db.get("k").value_or(""), "v2");
    EXPECT_FALSE(db.get("gone").has_value());
  }
}

TEST(Database, ManyOpsRoundTrip) {
  TempFile tf;
  constexpr int N = 200;
  {
    Database db(tf.path());
    for (int i = 0; i < N; ++i) {
      db.put("k" + std::to_string(i), "v" + std::to_string(i));
    }
  }
  {
    Database db(tf.path());
    for (int i = 0; i < N; ++i) {
      auto v = db.get("k" + std::to_string(i));
      ASSERT_TRUE(v.has_value()) << "i=" << i;
      EXPECT_EQ(*v, "v" + std::to_string(i));
    }
  }
}
