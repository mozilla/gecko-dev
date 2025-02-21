/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpserver = new HttpServer();

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
  httpserver.start(-1);

  registerCleanupFunction(async function () {
    await httpserver.stop();
  });
});

async function test(oaLoad, oaClear, shouldExist) {
  let port = httpserver.identity.primaryPort;
  info("Starting test with port " + port);

  let url = `http://localhost:${port}/test`;
  let chan = makeHTTPChannel(url);
  chan.loadInfo.originAttributes = oaLoad;
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });

  let cache_storage = getCacheStorage(
    "disk",
    Services.loadContextInfo.custom(false, oaLoad)
  );
  let exists = cache_storage.exists(make_uri(url), null);
  Assert.ok(exists, "Entry should be in cache");

  Services.cache2.clearOriginAttributes(JSON.stringify(oaClear));

  // clearOriginAttributes is async, so we block on cacheIOThread
  await new Promise(resolve => {
    syncWithCacheIOThread(resolve, true);
  });

  let existsAgain = cache_storage.exists(make_uri(url), null);
  Assert.equal(
    existsAgain,
    shouldExist,
    shouldExist ? "Entry should be in cache" : "Entry should not be in cache"
  );
}

add_task(async function test_clear_cache_with_usercontext_oa() {
  await test({ userContextId: 0 }, { userContextId: 0 }, false);
});

add_task(async function test_clear_cache_with_usercontext_oa() {
  await test({ userContextId: 0 }, { userContextId: 1 }, true);
});

add_task(
  async function test_clear_cache_with_usercontext_oa_across_partition() {
    await test(
      { userContextId: 0, partitionKey: "(https,example.com)" },
      { userContextId: 0 },
      true
    );
  }
);
