/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

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
});

add_task(async function test_clear_cache_with_usercontext_oa() {
  let port = httpserver.identity.primaryPort;
  info("Starting test with port " + port);

  let url = `http://localhost:${port}/test`;
  let chan = makeHTTPChannel(url);
  chan.loadInfo.originAttributes = { userContextId: 0 };
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });

  let cache_storage = getCacheStorage("disk");
  let exists = cache_storage.exists(make_uri(url), null);
  Assert.ok(exists, "Entry should be in cache");

  Services.cache2.clearOriginAttributes(JSON.stringify({ userContextId: 0 }));
  await new Promise(resolve => lazy.setTimeout(resolve, 0)); // clearOriginAttributes does not block

  let existsAgain = cache_storage.exists(make_uri(url), null);
  Assert.ok(!existsAgain, "Entry should not be in cache");

  await httpserver.stop();
});
