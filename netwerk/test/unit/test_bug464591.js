// 1.percent-encoded IDN that contains blacklisted character should be converted
//   to punycode, not UTF-8 string
// 2.only hostname-valid percent encoded ASCII characters should be decoded
// 3.IDN convertion must not bypassed by %00

"use strict";

let reference = [
  [
    "www.example.com%e2%88%95www.mozill%d0%b0.com%e2%81%84www.mozilla.org",
    "www.example.xn--comwww-re3c.xn--mozill-8nf.xn--comwww-rq0c.mozilla.org",
  ],
];

let badURIs = [
  ["www.mozill%61%2f.org"], // a slash is not valid in the hostname
  ["www.e%00xample.com%e2%88%95www.mozill%d0%b0.com%e2%81%84www.mozill%61.org"],
];

function stringToURL(str) {
  return Cc["@mozilla.org/network/standard-url-mutator;1"]
    .createInstance(Ci.nsIStandardURLMutator)
    .init(Ci.nsIStandardURL.URLTYPE_AUTHORITY, 80, str, "UTF-8", null)
    .finalize()
    .QueryInterface(Ci.nsIURL);
}

function run_test() {
  for (let i = 0; i < reference.length; ++i) {
    try {
      let result = stringToURL("http://" + reference[i][0]).host;
      equal(result, reference[i][1]);
    } catch (e) {
      ok(false, "Error testing " + reference[i][0]);
    }
  }

  for (let i = 0; i < badURIs.length; ++i) {
    Assert.throws(
      () => {
        stringToURL("http://" + badURIs[i][0]).host;
      },
      /NS_ERROR_MALFORMED_URI/,
      "bad escaped character"
    );
  }
}
