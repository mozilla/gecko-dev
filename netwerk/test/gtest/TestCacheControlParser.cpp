#include "gtest/gtest.h"

#include "CacheControlParser.h"

using namespace mozilla;
using namespace mozilla::net;

TEST(TestCacheControlParser, NegativeMaxAge)
{
  CacheControlParser cc(
      "no-store,no-cache,max-age=-3600,max-stale=7,private"_ns);
  ASSERT_TRUE(cc.NoStore());
  ASSERT_TRUE(cc.NoCache());
  uint32_t max_age{2};
  ASSERT_FALSE(cc.MaxAge(&max_age));
  ASSERT_EQ(max_age, 0U);
  uint32_t max_stale;
  ASSERT_TRUE(cc.MaxStale(&max_stale));
  ASSERT_EQ(max_stale, 7U);
  ASSERT_TRUE(cc.Private());
}

TEST(TestCacheControlParser, EmptyMaxAge)
{
  CacheControlParser cc("no-store,no-cache,max-age,max-stale=77,private"_ns);
  ASSERT_TRUE(cc.NoStore());
  ASSERT_TRUE(cc.NoCache());
  uint32_t max_age{5};
  ASSERT_FALSE(cc.MaxAge(&max_age));
  ASSERT_EQ(max_age, 0U);
  uint32_t max_stale;
  ASSERT_TRUE(cc.MaxStale(&max_stale));
  ASSERT_EQ(max_stale, 77U);
  ASSERT_TRUE(cc.Private());
}

TEST(TestCacheControlParser, NegaTiveMaxStale)
{
  CacheControlParser cc(
      "no-store,no-cache,max-age=3600,max-stale=-222,private"_ns);
  ASSERT_TRUE(cc.NoStore());
  ASSERT_TRUE(cc.NoCache());
  uint32_t max_age;
  ASSERT_TRUE(cc.MaxAge(&max_age));
  ASSERT_EQ(max_age, 3600U);
  uint32_t max_stale;
  ASSERT_TRUE(cc.MaxStale(&max_stale));
  ASSERT_EQ(max_stale, PR_UINT32_MAX);
  ASSERT_TRUE(cc.Private());
}

TEST(TestCacheControlParser, EmptyMaxStale)
{
  CacheControlParser cc("no-store,no-cache,max-age=3600,max-stale,private"_ns);
  ASSERT_TRUE(cc.NoStore());
  ASSERT_TRUE(cc.NoCache());
  uint32_t max_age;
  ASSERT_TRUE(cc.MaxAge(&max_age));
  ASSERT_EQ(max_age, 3600U);
  uint32_t max_stale;
  ASSERT_TRUE(cc.MaxStale(&max_stale));
  ASSERT_EQ(max_stale, PR_UINT32_MAX);
  ASSERT_TRUE(cc.Private());
}
// cache control parser case-insensitive`
TEST(TestCacheControlParser, CaseInsensitive)
{
  CacheControlParser cc(
      "No-Store,No-Cache,Max-Age=3600,Max-Stale=7,Private,Min-Fresh=1,Stale-while-revalidate=3"_ns);
  ASSERT_TRUE(cc.NoStore());
  ASSERT_TRUE(cc.NoCache());
  uint32_t max_age;
  ASSERT_TRUE(cc.MaxAge(&max_age));
  ASSERT_EQ(max_age, 3600U);
  uint32_t max_stale;
  ASSERT_TRUE(cc.MaxStale(&max_stale));
  ASSERT_EQ(max_stale, 7U);
  ASSERT_TRUE(cc.Private());
  uint32_t min_fresh;
  ASSERT_TRUE(cc.MinFresh(&min_fresh));
  ASSERT_EQ(min_fresh, 1U);
  uint32_t stale_while_revalidate;
  ASSERT_TRUE(cc.StaleWhileRevalidate(&stale_while_revalidate));
  ASSERT_EQ(stale_while_revalidate, 3U);

  // Test case-insensitive capitalization
  CacheControlParser cc2(
      "NO-STORE,NO-CACHE,MAX-AGE=2600,MAX-STALE=12,PUBLIC,MIN-FRESH=3,STALE-WHILE-REVALIDATE=8"_ns);
  ASSERT_TRUE(cc2.NoStore());
  ASSERT_TRUE(cc2.NoCache());
  ASSERT_TRUE(cc2.MaxAge(&max_age));
  ASSERT_EQ(max_age, 2600U);
  ASSERT_TRUE(cc2.MaxStale(&max_stale));
  ASSERT_EQ(max_stale, 12u);
  ASSERT_TRUE(cc2.Public());
  ASSERT_TRUE(cc2.MinFresh(&min_fresh));
  ASSERT_EQ(min_fresh, 3U);
  ASSERT_TRUE(cc2.StaleWhileRevalidate(&stale_while_revalidate));
  ASSERT_EQ(stale_while_revalidate, 8U);
}
