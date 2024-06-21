/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  for (const [host, shouldKnowTld] of [
    ["example.com", true], // https://www.iana.org/domains/root/db/com.html
    ["example.local", false],
    ["example.vermögensberatung", true], // https://www.iana.org/domains/root/db/xn--vermgensberatung-pwb.html
    ["example.xn--vermgensberatung-pwb", true],
    ["example.löcal", false],
    ["example.xn--lcal-5qa", false],
    ["localhost", false],
    ["com", true],
    ["za", false],
    ["co.za", true],
    ["example.co.za", true],
    ["example.com.", true],
    ["example.local.", false],
  ]) {
    Assert.equal(
      Services.eTLD.hasKnownPublicSuffixFromHost(host),
      shouldKnowTld,
      `"${host}" should ${
        shouldKnowTld ? " " : "not "
      }have a known public suffix`
    );
    Assert.equal(
      Services.eTLD.hasKnownPublicSuffix(Services.io.newURI("http://" + host)),
      shouldKnowTld,
      `"http://${host}" should ${
        shouldKnowTld ? " " : "not "
      }have a known public suffix`
    );
  }
});
