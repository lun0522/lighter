//
//  ref_count_test.cc
//

#include "lighter/base/ref_count.h"

#include <gtest/gtest.h>

#include <utility>

namespace lighter::base {
namespace {

struct ImmediateReleaseItem {
  static inline int live_count = 0;

  explicit ImmediateReleaseItem(int v) : value{v} { ++live_count; }
  ~ImmediateReleaseItem() { --live_count; }

  int value;
};

struct PooledReleaseItem {
  static inline int live_count = 0;

  explicit PooledReleaseItem(int v) : value{v} { ++live_count; }
  ~PooledReleaseItem() { --live_count; }

  int value;
};

TEST(RefCountedObjectTest, DeduplicatesAndReleasesImmediatelyWithoutPool) {
  using Handle = RefCountedObject<ImmediateReleaseItem>;
  EXPECT_EQ(ImmediateReleaseItem::live_count, 0);
  EXPECT_FALSE(Handle::has_active_auto_release_pool());

  {
    Handle a = Handle::Get("alpha", 10);
    EXPECT_EQ(ImmediateReleaseItem::live_count, 1);
    EXPECT_EQ(a->value, 10);

    Handle b = Handle::Get("alpha", 99);
    EXPECT_EQ(ImmediateReleaseItem::live_count, 1);
    EXPECT_EQ(&*a, &*b);
    EXPECT_EQ((*b).value, 10);

    Handle moved = std::move(b);
    EXPECT_EQ(moved->value, 10);
    EXPECT_EQ(ImmediateReleaseItem::live_count, 1);
  }

  EXPECT_EQ(ImmediateReleaseItem::live_count, 0);
}

TEST(RefCountedObjectTest, AutoReleasePoolDefersDestructionAcrossNestedScopes) {
  using Handle = RefCountedObject<PooledReleaseItem>;
  EXPECT_EQ(PooledReleaseItem::live_count, 0);

  {
    Handle::AutoReleasePool outer_pool;
    EXPECT_TRUE(Handle::has_active_auto_release_pool());

    {
      Handle::AutoReleasePool inner_pool;
      Handle item = Handle::Get("beta", 42);
      EXPECT_EQ(PooledReleaseItem::live_count, 1);
      EXPECT_EQ(item->value, 42);
    }

    // Inner pool exited and handle dropped, but outer pool keeps it cached.
    EXPECT_TRUE(Handle::has_active_auto_release_pool());
    EXPECT_EQ(PooledReleaseItem::live_count, 1);

    Handle reused = Handle::Get("beta", 999);
    EXPECT_EQ(reused->value, 42);
  }

  EXPECT_FALSE(Handle::has_active_auto_release_pool());
  EXPECT_EQ(PooledReleaseItem::live_count, 0);
}

}  // namespace
}  // namespace lighter::base
