// Tests nsIIDNService

"use strict";

const idnService = Cc["@mozilla.org/network/idn-service;1"].getService(
  Ci.nsIIDNService
);

add_task(async function test_simple() {
  function isACE(domain) {
    return domain.startsWith("xn--") || domain.indexOf(".xn--") > -1;
  }

  let reference = [
    // The 3rd element indicates whether the second element
    // is ACE-encoded
    ["asciihost", "asciihost", false],
    ["b\u00FCcher", "xn--bcher-kva", true],
  ];

  for (var i = 0; i < reference.length; ++i) {
    dump("Testing " + reference[i] + "\n");
    // We test the following:
    // - Converting UTF-8 to ACE and back gives us the expected answer
    // - Converting the ASCII string UTF-8 -> ACE leaves the string unchanged
    // - isACE returns true when we expect it to (third array elem true)
    Assert.equal(idnService.convertUTF8toACE(reference[i][0]), reference[i][1]);
    Assert.equal(idnService.convertUTF8toACE(reference[i][1]), reference[i][1]);
    Assert.equal(idnService.convertACEtoUTF8(reference[i][1]), reference[i][0]);
    Assert.equal(isACE(reference[i][1]), reference[i][2]);
  }
});
