/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  profiles,
  PROFILE_CONSTANTS,
} = require("resource://devtools/client/shared/components/throttling/profiles.js");

const offlineProfile = profiles.find(
  profile => profile.id === PROFILE_CONSTANTS.OFFLINE
);

const CACHED_URL = STATUS_CODES_SJS + "?sts=ok&cached&test_cached";

add_task(async function () {
  const { monitor } = await initNetMonitor(SIMPLE_URL, { requestCount: 1 });
  const { connector } = monitor.panelWin;
  const { updateNetworkThrottling } = connector;

  // Throttle the network before entering the content process
  await pushPref("devtools.cache.disabled", false);
  await updateNetworkThrottling(true, offlineProfile);

  // Clear the cache beforehand
  Services.cache2.clear();

  // The request is blocked since the profile is offline
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [CACHED_URL],
    async url => {
      try {
        await content.fetch(url);
        ok(false, "Should not load since profile is offline");
      } catch (err) {
        is(err.name, "TypeError", "Should fail since profile is offline");
      }
    }
  );

  // Disable throttling
  await pushPref("devtools.cache.disabled", false);
  await updateNetworkThrottling(false);

  // Fetch to prime the cache
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [CACHED_URL],
    async url => {
      await content.fetch(url);
    }
  );

  // Set profile to offline again
  await pushPref("devtools.cache.disabled", false);
  await updateNetworkThrottling(true, offlineProfile);

  // Check that cached resource loaded
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [CACHED_URL],
    async url => {
      await content.fetch(url);
    }
  );

  await teardown(monitor);
});
