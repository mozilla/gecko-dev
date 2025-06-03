/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const userContextId1 = 3;
const userContextId2 = 5;
const url = "http://foo.mozilla.org/";
const url2 = "http://foo2.mozilla.org/";

add_task(async function test_openTabs() {
  UrlbarProviderOpenTabs.registerOpenTab(url, userContextId1, null, false);
  UrlbarProviderOpenTabs.registerOpenTab(url, userContextId1, null, false);
  UrlbarProviderOpenTabs.registerOpenTab(url2, userContextId1, null, false);
  UrlbarProviderOpenTabs.registerOpenTab(url, userContextId2, null, false);
  Assert.deepEqual(
    [url, url2],
    UrlbarProviderOpenTabs.getOpenTabUrlsForUserContextId(userContextId1),
    "Found all the expected tabs"
  );
  Assert.deepEqual(
    [url],
    UrlbarProviderOpenTabs.getOpenTabUrlsForUserContextId(userContextId2),
    "Found all the expected tabs"
  );
  await PlacesUtils.promiseLargeCacheDBConnection();
  await UrlbarProviderOpenTabs.promiseDBPopulated;
  Assert.deepEqual(
    [
      { url, userContextId: userContextId1, groupId: null, count: 2 },
      { url: url2, userContextId: userContextId1, groupId: null, count: 1 },
      { url, userContextId: userContextId2, groupId: null, count: 1 },
    ],
    await UrlbarProviderOpenTabs.getDatabaseRegisteredOpenTabsForTests(),
    "Found all the expected tabs"
  );

  await UrlbarProviderOpenTabs.unregisterOpenTab(
    url2,
    userContextId1,
    null,
    false
  );
  Assert.deepEqual(
    [url],
    UrlbarProviderOpenTabs.getOpenTabUrlsForUserContextId(userContextId1),
    "Found all the expected tabs"
  );
  await UrlbarProviderOpenTabs.unregisterOpenTab(
    url,
    userContextId1,
    null,
    false
  );
  Assert.deepEqual(
    [url],
    UrlbarProviderOpenTabs.getOpenTabUrlsForUserContextId(userContextId1),
    "Found all the expected tabs"
  );
  Assert.deepEqual(
    [
      { url, userContextId: userContextId1, groupId: null, count: 1 },
      { url, userContextId: userContextId2, groupId: null, count: 1 },
    ],
    await UrlbarProviderOpenTabs.getDatabaseRegisteredOpenTabsForTests(),
    "Found all the expected tabs"
  );

  let context = createContext();
  let matchCount = 0;
  let callback = function (provider, match) {
    matchCount++;
    Assert.ok(
      provider instanceof UrlbarProviderOpenTabs,
      "Got the expected provider"
    );
    Assert.equal(
      match.type,
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      "Got the expected result type"
    );
    Assert.equal(match.payload.url, url, "Got the expected url");
    Assert.equal(match.payload.title, undefined, "Got the expected title");
  };

  let provider = new UrlbarProviderOpenTabs();
  await provider.startQuery(context, callback);
  Assert.equal(matchCount, 2, "Found the expected number of matches");
  // Sanity check that this doesn't throw.
  provider.cancelQuery(context);
  await UrlbarProviderOpenTabs.unregisterOpenTab(
    url,
    userContextId1,
    null,
    false
  );
  await UrlbarProviderOpenTabs.unregisterOpenTab(
    url,
    userContextId2,
    null,
    false
  );
});

add_task(async function test_openTabs_mixedtype_input() {
  // Passing the userContextId as a string, rather than a number, is a fairly
  // common mistake, check the API handles both properly.
  Assert.deepEqual(
    [],
    UrlbarProviderOpenTabs.getOpenTabUrls(1),
    "Found all the expected tabs"
  );
  Assert.deepEqual(
    [],
    UrlbarProviderOpenTabs.getOpenTabUrls(2),
    "Found all the expected tabs"
  );
  UrlbarProviderOpenTabs.registerOpenTab(url, 1, null, false);
  UrlbarProviderOpenTabs.registerOpenTab(url, "2", null, false);
  Assert.deepEqual(
    [url],
    UrlbarProviderOpenTabs.getOpenTabUrlsForUserContextId(1),
    "Found all the expected tabs"
  );
  Assert.deepEqual(
    [url],
    UrlbarProviderOpenTabs.getOpenTabUrlsForUserContextId(2),
    "Found all the expected tabs"
  );
  Assert.deepEqual(
    UrlbarProviderOpenTabs.getOpenTabUrlsForUserContextId(1),
    UrlbarProviderOpenTabs.getOpenTabUrlsForUserContextId("1"),
    "Also check getOpenTabs adapts to the argument type"
  );
  UrlbarProviderOpenTabs.unregisterOpenTab(url, "1", null, false);
  UrlbarProviderOpenTabs.unregisterOpenTab(url, 2, null, false);
  Assert.deepEqual(
    [],
    UrlbarProviderOpenTabs.getOpenTabUrlsForUserContextId(1),
    "Found all the expected tabs"
  );
  Assert.deepEqual(
    [],
    UrlbarProviderOpenTabs.getOpenTabUrlsForUserContextId(2),
    "Found all the expected tabs"
  );
});

add_task(async function test_openTabs() {
  Assert.equal(
    0,
    UrlbarProviderOpenTabs.getOpenTabUrls().size,
    "Check there's no open tabs"
  );
  Assert.equal(
    0,
    UrlbarProviderOpenTabs.getOpenTabUrls(true).size,
    "Check there's no private open tabs"
  );
  await UrlbarProviderOpenTabs.registerOpenTab(
    url,
    userContextId1,
    null,
    false
  );
  await UrlbarProviderOpenTabs.registerOpenTab(
    url,
    userContextId2,
    null,
    false
  );
  await UrlbarProviderOpenTabs.registerOpenTab(url2, 0, null, true);
  Assert.equal(
    1,
    UrlbarProviderOpenTabs.getOpenTabUrls().size,
    "Check open tabs"
  );
  Assert.deepEqual(
    [userContextId1, userContextId2],
    Array.from(UrlbarProviderOpenTabs.getOpenTabUrls().get(url)),
    "Check the tab is in 2 userContextIds"
  );
  Assert.equal(
    1,
    UrlbarProviderOpenTabs.getOpenTabUrls(true).size,
    "Check open private tabs"
  );
  Assert.deepEqual(
    [-1],
    Array.from(UrlbarProviderOpenTabs.getOpenTabUrls(true).get(url2)),
    "Check the tab is in the private userContextId"
  );
  await UrlbarProviderOpenTabs.unregisterOpenTab(
    url,
    userContextId1,
    null,
    false
  );
  await UrlbarProviderOpenTabs.unregisterOpenTab(
    url,
    userContextId2,
    null,
    false
  );
  await UrlbarProviderOpenTabs.unregisterOpenTab(url2, 0, null, true);
});

add_task(async function test_openTabsInGroup() {
  let tabGroup = "1234567890-1";
  Assert.equal(
    0,
    UrlbarProviderOpenTabs.getOpenTabUrls().size,
    "Check there's no open tabs"
  );

  await UrlbarProviderOpenTabs.registerOpenTab(url2, 0, tabGroup, false);
  let expected = { count: 1, tabGroup, url: url2, userContextId: 0 };
  let result =
    await UrlbarProviderOpenTabs.getDatabaseRegisteredOpenTabsForTests();
  Assert.deepEqual(result, [expected], "Open tab is registered with group");

  UrlbarProviderOpenTabs.unregisterOpenTab(url2, 0, null, false);
  result = await UrlbarProviderOpenTabs.getDatabaseRegisteredOpenTabsForTests();
  Assert.deepEqual(
    result,
    [expected],
    "Open tab is still registered even when unregistering same URL/contextid not in group"
  );

  UrlbarProviderOpenTabs.unregisterOpenTab(url2, 0, tabGroup, false);
  result = await UrlbarProviderOpenTabs.getDatabaseRegisteredOpenTabsForTests();
  Assert.deepEqual(result, [], "Open tab is unregistered");
});
