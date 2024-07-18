"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

add_task(async function test_runtime_getURL() {
  function background() {
    const origin = location.origin;
    function checkURL(input, expectedOutput, extraDescription = "") {
      browser.test.assertEq(
        expectedOutput,
        browser.runtime.getURL(input),
        `Expected result for runtime.getURL(${input}) : ${extraDescription}`
      );
    }

    // Common cases: leading / is optional.
    checkURL("test.txt", `${origin}/test.txt`, "Leading / is optional");
    checkURL("/test.txt", `${origin}/test.txt`, "With leading /");

    // Test cases from https://github.com/w3c/webextensions/issues/281

    // Case 1: "passing non-string parameter"
    browser.test.assertThrows(
      () => browser.runtime.getURL(200),
      "Incorrect argument types for runtime.getURL.",
      "getURL() with non-string should throw"
    );
    browser.test.assertThrows(
      () => browser.runtime.getURL(),
      "Incorrect argument types for runtime.getURL.",
      "getURL() requires a parameter"
    );

    // Case 2: "passing lesser known utf-8 characters"
    checkURL("Ω", `${origin}/Ω`, "Lesser known utf-8 character");

    // Case 3: "passing a full external URL"
    checkURL("https://example.com/", `${origin}/https://example.com/`);

    // Case 4: "passing a full extension-owned URL"
    // Note: Firefox has historically returned the parsed URL when the input is
    // the moz-extension:-URL. To minimize the odds of regressions, we preserve
    // the behavior. Note: Chrome returns `${origin}/${origin}`.
    checkURL(`${origin}/`, `${origin}/`, "");
    checkURL(location.href, location.href, "Full moz-extension URL");

    // Case 5: "passing URL which is exactly //"
    checkURL("//", `${origin}//`);

    // Case 6: "passing URL which starts with or is three slashes ///"
    checkURL("///", `${origin}///`);

    // Case 7: "passing URL which starts with exactly two slashes and at least
    //          one non-slash character"
    checkURL("//example", `${origin}//example`);

    // Case 8: "passing URL which is exactly ."
    checkURL(".", `${origin}/.`);

    // Case 9: "passing URL which starts with ./"
    checkURL("././/example", `${origin}/././/example`);

    // Case 10: "passing URL which contains ../"
    checkURL("../../example/..//test/", `${origin}/../../example/..//test/`);

    // Case 11: "../ artifact edge-cases"
    checkURL("../", `${origin}/../`);
    checkURL("..", `${origin}/..`);
    checkURL("/.././", `${origin}/.././`);

    // More edge cases not covered above.

    // moz-extension:-URL, not this extension.
    checkURL("moz-extension://uuid/", `${origin}/moz-extension://uuid/`);
    // moz-extension:-origin (missing trailing slash, not a URL).
    checkURL(origin, `${origin}/${origin}`, "moz-extension without path slash");

    browser.test.sendMessage("check_origin", origin);
  }
  let extension = ExtensionTestUtils.loadExtension({ background });
  await extension.startup();
  equal(
    await extension.awaitMessage("check_origin"),
    `moz-extension://${extension.uuid}`,
    "Got expected base URL (origin)"
  );
  await extension.unload();
});

add_task(async function test_contentscript() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.runtime.onMessage.addListener(([url1, url2]) => {
        let url3 = browser.runtime.getURL("test_file.html");
        let url4 = browser.extension.getURL("test_file.html");

        browser.test.assertTrue(url1 !== undefined, "url1 defined");

        browser.test.assertTrue(
          url1.startsWith("moz-extension://"),
          "url1 has correct scheme"
        );
        browser.test.assertTrue(
          url1.endsWith("test_file.html"),
          "url1 has correct leaf name"
        );

        browser.test.assertEq(url1, url2, "url2 matches");
        browser.test.assertEq(url1, url3, "url3 matches");
        browser.test.assertEq(url1, url4, "url4 matches");

        browser.test.notifyPass("geturl");
      });
    },

    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/data/file_sample.html"],
          js: ["content_script.js"],
          run_at: "document_idle",
        },
      ],
    },

    files: {
      "content_script.js"() {
        let url1 = browser.runtime.getURL("test_file.html");
        let url2 = browser.extension.getURL("test_file.html");
        browser.runtime.sendMessage([url1, url2]);
      },
    },
  });
  // Turn off warning as errors to pass for deprecated APIs
  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );

  await extension.awaitFinish("geturl");

  await contentPage.close();

  await extension.unload();
  ExtensionTestUtils.failOnSchemaWarnings(true);
});
