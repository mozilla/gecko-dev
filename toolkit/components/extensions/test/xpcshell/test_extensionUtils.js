/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  ExtensionUtils: { isExtensionUrl },
} = ChromeUtils.importESModule("resource://gre/modules/ExtensionUtils.sys.mjs");

add_task(function test_isExtensionUrl() {
  const badUrl = "extension://test";
  const goodUrl = "moz-extension://test";

  let expectedResults = [
    {
      url: "",
      expected: false,
      msg: "Empty string is not a moz-extension scheme url.",
    },
    {
      url: badUrl,
      expected: false,
      msg: "String is not a moz-extension scheme url.",
    },
    {
      url: goodUrl,
      expected: true,
      msg: "String is a moz-extension scheme url.",
    },
    {
      url: new URL(badUrl),
      expected: false,
      msg: "URL is not a moz-extension scheme url.",
    },
    {
      url: new URL(goodUrl),
      expected: true,
      msg: "URL is a moz-extension scheme url.",
    },
    {
      url: Services.io.newURI(badUrl),
      expected: false,
      msg: "nsIURI is not a moz-extension scheme url.",
    },
    {
      url: Services.io.newURI(goodUrl),
      expected: true,
      msg: "nsIURI is a moz-extension scheme url.",
    },
    {
      url: Services.scriptSecurityManager.createContentPrincipal(
        Services.io.newURI(badUrl),
        {}
      ),
      expected: false,
      msg: "nsIPrincipal is not a moz-extension scheme url.",
    },
    {
      url: Services.scriptSecurityManager.createContentPrincipal(
        Services.io.newURI(goodUrl),
        {}
      ),
      expected: true,
      msg: "nsIPrincipal is a moz-extension scheme url.",
    },
    {
      url: null,
      expected: false,
      msg: "null is not a moz-extension scheme url.",
    },
    {
      url: undefined,
      expected: false,
      msg: "null is not a moz-extension scheme url.",
    },
  ];

  for (const result of expectedResults) {
    const { url, expected, msg } = result;
    Assert.equal(isExtensionUrl(url), expected, msg);
  }
});
