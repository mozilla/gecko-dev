/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

add_setup(async function () {
  await http3_setup_tests("h3");
});

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

async function doTestTimings() {
  Services.obs.notifyObservers(null, "net:prune-all-connections");

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));

  let chan = makeChan(`https://foo.example.com`);
  let [req] = await channelOpenPromise(chan);
  let httpVersion = "";
  try {
    httpVersion = req.protocolVersion;
  } catch (e) {}
  Assert.equal(httpVersion, "h3");

  let timing = req.QueryInterface(Ci.nsITimedChannel);
  Assert.ok(timing.connectStartTime > 0);
  Assert.equal(timing.connectStartTime, timing.secureConnectionStartTime);
  Assert.ok(timing.connectEndTime > timing.connectStartTime);
}

add_task(async function test_connectStart_equals_secureConnectionStart() {
  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 0);
  await doTestTimings();

  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 6);
  await doTestTimings();
});
