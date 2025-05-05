/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Checks certain sequences of characters encoded properly in the URL

// As completely duplicating the list in losslessDecodeURI wouldn't make sense
// we just test a few examples from it.

add_task(async function () {
  let enc = encodeURIComponent;
  let tests = [
    ["https://example.com/   test", "https://example.com/%20%20 test"],
    [
      "https://example.com/   \u{0702}test",
      "https://example.com/%20%20 \u{0702}test",
    ],
    [
      "https://example.com/ \u{200C} \u{200C} test",
      "https://example.com/%20\u200C%20\u200C test",
    ],
    [
      "https://example.com/\u{2800}test",
      `https://example.com/${enc("\u{2800}")}test`,
    ],
    [
      "https://example.com/\u{000B}test",
      `https://example.com/${enc("\u{000B}")}test`,
    ],
    [
      "https://example.com/\u{200D}\u{200C}\u{200D}test",
      "https://example.com/\u{200D}\u{200C}\u{200D}test",
    ],
    [
      "javascript: (() => { alert('test'); } })();",
      "javascript: (() => { alert('test'); } })();",
    ],
    ["https://example.com/ %3Dtest", `https://example.com/ %3Dtest`],
    [
      "https://example.com/\u{E012A}test",
      `https://example.com/${enc("\u{E012A}")}test`,
    ],
    [
      "https://example.com/a\u{200C}\u{200D}b",
      "https://example.com/a\u{200C}\u{200D}b",
    ],
    [
      "https://example.com/a\u{00AD}\u{034F}\u{061C}b",
      `https://example.com/a${enc("\u{00AD}\u{034F}\u{061C}")}b`,
    ],
  ];

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:robots",
    },
    function () {
      for (let [url, expected] of tests) {
        info("testing: " + url);
        gURLBar.setURI(Services.io.newURI(url), false, true);
        Assert.equal(gURLBar.untrimmedValue, expected);
      }
    }
  );
});
