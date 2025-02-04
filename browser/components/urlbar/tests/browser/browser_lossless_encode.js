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
      "https://example.com/   \u0702test",
      "https://example.com/%20%20 \u0702test",
    ],
    [
      "https://example.com/ \u200C \u200C test",
      "https://example.com/%20\u200C%20\u200C test",
    ],
    [
      "https://example.com/\u2800test",
      `https://example.com/${enc("\u2800")}test`,
    ],
    [
      "https://example.com/\u000Btest",
      `https://example.com/${enc("\u000B")}test`,
    ],
    [
      "https://example.com/\u200D\u200C\u200Dtest",
      `https://example.com/\u200D\u200C\u200Dtest`,
    ],
    [
      "javascript: (() => { alert('test'); } })();",
      "javascript: (() => { alert('test'); } })();",
    ],
    ["https://example.com/ %3Dtest", `https://example.com/ %3Dtest`],
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
