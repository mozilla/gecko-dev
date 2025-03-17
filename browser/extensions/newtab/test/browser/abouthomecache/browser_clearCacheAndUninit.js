/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that any pre-existing cache is blown away, and that no new caches are
 * created after clearCacheAndUninit is called, until the simulated restart.
 */
add_task(async function test_clearCacheAndUninit() {
  await withFullyLoadedAboutHome(async browser => {
    // First, ensure that a pre-existing cache exists.
    await simulateRestart(browser);

    // Now blow away the cache and uninitialize.
    AboutHomeStartupCache.clearCacheAndUninit();

    // Simulate that the newtab was updated such that a cache write would
    // normally occur at shutdown.
    AboutHomeStartupCache.onPreloadedNewTabMessage();

    // We don't need to shutdown write or ensure the cache wins the race,
    // since we expect the cache to be blown away because of the call to
    // clearCacheAndUninit.
    await simulateRestart(browser, {
      ensureCacheWinsRace: false,
    });

    await ensureDynamicAboutHome(
      browser,
      AboutHomeStartupCache.CACHE_RESULT_SCALARS.DOES_NOT_EXIST
    );
  });
});
