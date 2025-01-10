/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function setupChannel(uri) {
  var chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  });
  chan.QueryInterface(Ci.nsIFileChannel);
  return chan;
}

add_task(async function test() {
  setupChannel("file:///path");
  Assert.ok(
    true,
    "Should be able to create channel from file URL without hostname"
  );

  setupChannel("file://example.com/path");
  Assert.ok(
    true,
    "Should be able to create channel from file URL with hostname"
  );

  await Assert.rejects(
    fetch("file:///path"),
    /TypeError: NetworkError when attempting to fetch resource./
  );

  await Assert.rejects(
    fetch("file://example.com/path"),
    /TypeError: NetworkError when attempting to fetch resource./
  );
});
