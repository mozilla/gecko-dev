"use strict";

// ExtensionContent.sys.mjs needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function test_get_preferred_system_languages() {
  function background() {
    browser.test.onMessage.addListener((msg, expected) => {
      browser.test.assertEq("expect-results", msg, "Expected message");
      browser.i18n.getPreferredSystemLanguages().then(results => {
        browser.test.assertDeepEq(
          expected,
          results,
          "got expected languages in background"
        );

        const isNonEmptyArray = Array.isArray(results) && !!results.length;
        browser.test.assertTrue(isNonEmptyArray, "Is non empty array");

        const canonicalLocales = Intl.getCanonicalLocales(results);
        browser.test.assertDeepEq(
          results,
          canonicalLocales,
          "Result gives canonical versions"
        );

        browser.test.sendMessage("background-done");
      });
    });
  }

  function content() {
    browser.test.onMessage.addListener((msg, expected) => {
      browser.test.assertEq("expect-results", msg, "Expected message");
      browser.i18n.getPreferredSystemLanguages().then(results => {
        browser.test.assertDeepEq(
          expected,
          results,
          "got expected languages in contentScript"
        );

        browser.test.sendMessage("content-done");
      });
    });
    browser.test.sendMessage("content-loaded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/*/file_sample.html"],
          run_at: "document_start",
          js: ["content_script.js"],
        },
      ],
    },

    background: `(${background})()`,

    files: {
      "content_script.js": `(${content})()`,
    },
  });

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.startup();
  await extension.awaitMessage("content-loaded");

  const localesWithoutOverwritingPreferences = Cc[
    "@mozilla.org/intl/ospreferences;1"
  ].getService(Ci.mozIOSPreferences).systemLocales;

  extension.sendMessage("expect-results", localesWithoutOverwritingPreferences);

  await extension.awaitMessage("background-done");
  await extension.awaitMessage("content-done");

  await contentPage.close();

  await extension.unload();
});
