/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_prio_header_override_forbid() {
  var request = NetUtil.newChannel({
    uri: "https://foo.example.com/priority_mirror",
    loadUsingSystemPrincipal: true,
  });
  var chan = request.QueryInterface(Ci.nsIHttpChannel);

  // Manually set up priority header.
  chan.setRequestHeader("Priority", "foo", false);
  Assert.equal(chan.getRequestHeader("Priority"), "foo");

  var cos = chan.QueryInterface(Ci.nsIClassOfService);
  Assert.equal(cos.getRequestHeader("Priority"), "foo");

  // Update priority header preferences to true
  Services.prefs.setBoolPref("network.http.http3.priority", true);
  Services.prefs.setBoolPref("network.http.priority_header.enabled", true);

  // Configure the channel with flags
  cos.addClassFlags(Ci.nsIClassOfService.Leader);

  var observer = {
    observe() {
      chan.cancel(Cr.NS_BINDING_ABORTED);
      Services.obs.removeObserver(this, "http-on-modify-request");
      Assert.equal(chan.getRequestHeader("Priority"), "foo");
    },
  };
  Services.obs.addObserver(observer, "http-on-modify-request");

  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
  });
});
