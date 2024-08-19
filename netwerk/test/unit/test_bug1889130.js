/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  Test that channels blocks 0.0.0.0 ip address
 */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

const ip = "0.0.0.0";
let httpserver = new HttpServer();

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return `http://${ip}:${httpserver.identity.primaryPort}/`;
});

function plainResponse(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/html");
  response.setHeader("Content-Length", "2");
  response.bodyOutputStream.write("Ok", "Ok".length);
}

add_setup(function () {
  httpserver.registerPathHandler("/", plainResponse);
  httpserver._start(-1, ip);
  httpserver.identity.setPrimary(
    "http",
    "0.0.0.0",
    httpserver.identity.primaryPort
  );

  registerCleanupFunction(async () => {
    Services.prefs.setBoolPref("network.socket.ip_addr_any.disabled", true);
    await httpserver.stop(() => {});
  });
});

// this test verifies if we allow requests on 0.0.0.0 based on the pref
// network.socket.ip_addr_any.disabled. This is helpful for checking rollback of the bug
add_task(async function test_ipaddrany_allow() {
  Services.prefs.setBoolPref("network.socket.ip_addr_any.disabled", false);

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 5000));
  var chan = NetUtil.newChannel({
    uri: URL,
    loadUsingSystemPrincipal: true,
  });
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve));
  });
  Assert.equal(chan.status, Cr.NS_OK);
});

// this test verifies if we block requests on 0.0.0.0
add_task(async function test_ipaddrany_deny() {
  Services.prefs.setBoolPref("network.socket.ip_addr_any.disabled", true);

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 5000));

  var chan = NetUtil.newChannel({
    uri: URL,
    loadUsingSystemPrincipal: true,
  });
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
  });
  Assert.equal(chan.status, Cr.NS_ERROR_CONNECTION_REFUSED);
});
