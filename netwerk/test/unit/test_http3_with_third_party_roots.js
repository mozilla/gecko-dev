/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

var { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  }).QueryInterface(Ci.nsIHttpChannel);
  return chan;
}

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
    }
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

let h2Port;

add_setup(async function setup() {
  await http3_setup_tests("h3", true);
  Services.prefs.setBoolPref(
    "network.http.http3.disable_when_third_party_roots_found",
    true
  );

  registerCleanupFunction(async () => {
    http3_clear_prefs();

    Services.prefs.clearUserPref(
      "network.http.http3.disable_when_third_party_roots_found"
    );
    Services.prefs.clearUserPref(
      "network.http.http3.has_third_party_roots_found_in_automation"
    );
    Services.obs.notifyObservers(null, "network:reset_third_party_roots_check");
  });

  h2Port = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");
});

async function do_test(expectedVersion) {
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));

  let chan = makeChan(`https://foo.example.com:${h2Port}/`);
  let [req] = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  Assert.equal(req.protocolVersion, expectedVersion);
}

async function third_party_roots_check() {
  Services.obs.notifyObservers(null, "network:reset_third_party_roots_check");
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 3000));
}

add_task(async function test_http3_with_third_party_roots() {
  Services.prefs.setBoolPref(
    "network.http.http3.has_third_party_roots_found_in_automation",
    true
  );
  await third_party_roots_check();

  await do_test("h2");
});

add_task(async function test_http3_with_no_third_party_roots() {
  Services.prefs.setBoolPref(
    "network.http.http3.has_third_party_roots_found_in_automation",
    false
  );
  await third_party_roots_check();

  await do_test("h3");
});
