#include "test_helpers.h"
#include "wal/wal.h"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <string>

using namespace wdb::wal;
using wdb::test::TempFile;

TEST(Wal, EmptyFileReplaysNothing) {
  TempFile tf;
  Wal w(tf.path());
  auto recs = w.replay();
  EXPECT_TRUE(recs.empty());
}

TEST(Wal, AppendThenReplay) {
  TempFile tf;
  Wal w(tf.path());
  w.append(OpType::Put, "k1", "v1");
  w.append(OpType::Put, "k2", "v2");
  w.append(OpType::Del, "k1");
  w.sync();

  auto recs = w.replay();
  ASSERT_EQ(recs.size(), 3u);
  EXPECT_EQ(recs[0].op, OpType::Put);
  EXPECT_EQ(recs[0].key, "k1");
  EXPECT_EQ(recs[0].value, "v1");
  EXPECT_EQ(recs[1].op, OpType::Put);
  EXPECT_EQ(recs[1].key, "k2");
  EXPECT_EQ(recs[1].value, "v2");
  EXPECT_EQ(recs[2].op, OpType::Del);
  EXPECT_EQ(recs[2].key, "k1");
  EXPECT_EQ(recs[2].value, "");
}

TEST(Wal, ReplaySurvivesProcessRestart) {
  TempFile tf;
  {
    Wal w(tf.path());
    w.append(OpType::Put, "alpha", "1");
    w.append(OpType::Put, "beta", "2");
    w.sync();
  }
  {
    Wal w(tf.path());
    auto recs = w.replay();
    ASSERT_EQ(recs.size(), 2u);
    EXPECT_EQ(recs[0].key, "alpha");
    EXPECT_EQ(recs[1].key, "beta");
  }
}

TEST(Wal, TornTailDiscardedOnReplay) {
  TempFile tf;
  {
    Wal w(tf.path());
    w.append(OpType::Put, "good", "data");
    w.sync();
  }

  // Append a partial frame (truncated length prefix) by writing raw bytes.
  int fd = ::open(tf.path().c_str(), O_WRONLY | O_APPEND);
  ASSERT_GE(fd, 0);
  uint8_t garbage[2] = {0xAA, 0xBB};
  ::write(fd, garbage, sizeof(garbage));
  ::close(fd);

  Wal w(tf.path());
  auto recs = w.replay();
  ASSERT_EQ(recs.size(), 1u);
  EXPECT_EQ(recs[0].key, "good");

  // truncate_to_valid should drop the garbage bytes.
  w.truncate_to_valid();
  struct stat st {};
  ::stat(tf.path().c_str(), &st);
  EXPECT_EQ(st.st_size,
            4 + (1 + 2 + 4) + 4 + 8); // header+payload+crc for one rec
}

TEST(Wal, CrcMismatchStopsReplay) {
  TempFile tf;
  {
    Wal w(tf.path());
    w.append(OpType::Put, "one", "x");
    w.append(OpType::Put, "two", "y");
    w.sync();
  }
  // Flip a byte deep inside the file (in the second record's payload).
  int fd = ::open(tf.path().c_str(), O_RDWR);
  ASSERT_GE(fd, 0);
  struct stat st {};
  ::fstat(fd, &st);
  // Read second record offset: first frame = 4 + (1+2+4+3+1) + 4 = 19 bytes.
  off_t corrupt_at = 19 + 4 + 5; // somewhere in second payload's key bytes
  uint8_t flip = 0xFF;
  ::pwrite(fd, &flip, 1, corrupt_at);
  ::close(fd);

  Wal w(tf.path());
  auto recs = w.replay();
  EXPECT_EQ(recs.size(), 1u);
  EXPECT_EQ(recs[0].key, "one");
}

TEST(Wal, ResetTruncatesFile) {
  TempFile tf;
  Wal w(tf.path());
  w.append(OpType::Put, "k", "v");
  w.sync();
  EXPECT_GT(w.size_bytes(), 0u);
  w.reset();
  EXPECT_EQ(w.size_bytes(), 0u);
  EXPECT_TRUE(w.replay().empty());
}

TEST(Wal, EmptyValueOnPut) {
  TempFile tf;
  Wal w(tf.path());
  w.append(OpType::Put, "k", "");
  w.sync();
  auto recs = w.replay();
  ASSERT_EQ(recs.size(), 1u);
  EXPECT_EQ(recs[0].op, OpType::Put);
  EXPECT_EQ(recs[0].key, "k");
  EXPECT_EQ(recs[0].value, "");
}

TEST(Wal, LargeKeyAndValueRoundTrip) {
  TempFile tf;
  Wal w(tf.path());
  std::string k(1024, 'k');
  std::string v(10000, 'v');
  w.append(OpType::Put, k, v);
  w.sync();
  auto recs = w.replay();
  ASSERT_EQ(recs.size(), 1u);
  EXPECT_EQ(recs[0].key, k);
  EXPECT_EQ(recs[0].value, v);
}
