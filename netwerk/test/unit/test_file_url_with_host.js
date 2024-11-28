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

function run_test() {
  setupChannel("file:///path");
  Assert.ok(
    true,
    "Should be able to create channel from file URL without hostname"
  );

  // once we allow file URLs to have a hostname (bug 1507354) this will fail
  // at which point we should wrap the channel setup in a try-block
  // and assert in the catch
  setupChannel("file://example.com/path");
  Assert.ok(
    true,
    "Should be able to create channel from file URL with hostname"
  );
}
