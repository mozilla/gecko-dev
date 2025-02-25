/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

var httpserver = new HttpServer();
var port;

function make_uri(urlStr) {
  return Services.io.newURI(urlStr);
}

function serverHandler(_metadata, response) {
  const body = "hello world";
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(body, body.length);
}

add_setup(async function setup() {
  httpserver.registerPathHandler("/test", serverHandler);
  httpserver.registerPathHandler("/ignore", serverHandler);
  httpserver.start(-1);
  port = httpserver.identity.primaryPort;

  info("Starting test with port " + port);

  registerCleanupFunction(async function () {
    await httpserver.stop();
  });
});

async function load_get_principal(url, oa) {
  let chan = makeHTTPChannel(url);
  chan.loadInfo.originAttributes = oa;
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });
  return chan.loadInfo.loadingPrincipal;
}

async function test(oaLoad, oaClear, shouldExist) {
  // populate cache entry
  let url = `http://localhost:${port}/test`;
  await load_get_principal(url, oaLoad);

  let clearPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(`http://localhost:${port}/`),
    oaClear
  );
  // sanity check that the originAttributes match
  let expectedOa = { ...clearPrincipal.originAttributes, ...oaClear };
  Assert.deepEqual(clearPrincipal.originAttributes, expectedOa);

  let cache_storage = getCacheStorage(
    "disk",
    Services.loadContextInfo.custom(false, oaLoad)
  );
  let exists = cache_storage.exists(make_uri(url), null);
  Assert.ok(exists, "Entry should be in cache");

  Services.cache2.clearOrigin(clearPrincipal);

  // clearOrigin is async, so we block on cacheIOThread
  await new Promise(resolve => {
    syncWithCacheIOThread(resolve, true);
  });

  await TestUtils.waitForCondition(
    () => {
      let existsAgain = cache_storage.exists(make_uri(url), null);
      // times out when codition isn't met.
      return shouldExist == existsAgain;
    },
    shouldExist ? "Entry stay in cache" : "Entry get cleared from the cache"
  );
}

add_task(async function test_usercontextid_same() {
  await test({ userContextId: 0 }, { userContextId: 0 }, false);
});

add_task(async function test_usercontextid_same_nondefault() {
  await test({ userContextId: 1 }, { userContextId: 1 }, false);
});

add_task(async function test_usercontextid_different_1() {
  await test({ userContextId: 1 }, { userContextId: 0 }, true);
});

add_task(async function test_usercontextid_different_2() {
  await test({ userContextId: 0 }, { userContextId: 1 }, true);
});

add_task(async function test_privatebrowsingid_same() {
  await test({ privateBrowsingId: 1 }, { privateBrowsingId: 1 }, false);
});

add_task(async function test_privatebrowsingid_default() {
  await test({ privateBrowsingId: 0 }, { privateBrowsingId: 1 }, true);
});

add_task(async function test_privatebrowsingid_private() {
  await test({ privateBrowsingId: 1 }, { privateBrowsingId: 0 }, true);
});

add_task(async function test_partitionkey_same() {
  await test(
    { partitionKey: "(https,example.com)" },
    { partitionKey: "(https,example.com)" },
    false
  );
});

add_task(async function test_partitionkey_different_1() {
  await test(
    { partitionKey: "(http,example.com)" },
    { partitionKey: "(https,example.com)" },
    true
  );
});

add_task(async function test_partitionkey_different_2() {
  await test(
    { partitionKey: "(http,example.com)" },
    { partitionKey: "(https,example.com)" },
    true
  );
});

add_task(async function test_partitionkey_nonexisting_firstparty() {
  await test({ partitionKey: `(http,localhost,${port})` }, {}, true);
});

add_task(async function test_partitionkey_firstparty_nonexisting() {
  await test({}, { partitionKey: `(http,localhost,${port})` }, true);
});
