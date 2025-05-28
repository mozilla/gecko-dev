/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  info("Start netmonitor with the cache disabled");
  const { tab } = await initNetMonitor(SIMPLE_URL, {
    requestCount: 1,
    enableCache: false,
  });

  is(
    await getBrowsingContextDefaultLoadFlags(tab),
    Ci.nsIRequest.LOAD_BYPASS_CACHE,
    "Cache is disabled on the browsing context"
  );

  info("Open responsive design mode");
  await openRDM(tab);

  is(
    await getBrowsingContextDefaultLoadFlags(tab),
    Ci.nsIRequest.LOAD_BYPASS_CACHE,
    "Cache is still disabled on the browsing context after opening RDM"
  );

  info("Close responsive design mode");
  await closeRDM(tab);

  // wait for a bit so flags would have the time to be reset
  await wait(1000);

  is(
    await getBrowsingContextDefaultLoadFlags(tab),
    Ci.nsIRequest.LOAD_BYPASS_CACHE,
    "Cache is still disabled on the browsing context after closing RDM"
  );
});

function getBrowsingContextDefaultLoadFlags(tab) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => content.browsingContext.defaultLoadFlags
  );
}
