"use strict";

function run_test() {
  Services.prefs.setBoolPref("network.IDN_show_punycode", true);
  try {
    var uri = Services.io.newURI("http://xn--jos-dma.example.net.ch/");
    Assert.equal(uri.asciiHost, "xn--jos-dma.example.net.ch");
    Assert.equal(uri.displayHost, "xn--jos-dma.example.net.ch");
  } finally {
    Services.prefs.clearUserPref("network.IDN_show_punycode");
  }
}
