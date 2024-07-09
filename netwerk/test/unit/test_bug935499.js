"use strict";

function run_test() {
  var idnService = Cc["@mozilla.org/network/idn-service;1"].getService(
    Ci.nsIIDNService
  );

  Assert.throws(() => {
    idnService.convertToDisplayIDN("xn--");
  }, /MALFORMED/);
}
