"use strict";

// ExtensionContent.sys.mjs needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function test_contentscript_xrays() {
  async function contentScript() {
    let unwrapped = window.wrappedJSObject;

    browser.test.assertEq(
      "undefined",
      typeof test,
      "Should not have named X-ray property access"
    );
    browser.test.assertEq(
      undefined,
      window.test,
      "Should not have named X-ray property access"
    );
    browser.test.assertEq(
      "object",
      typeof unwrapped.test,
      "Should always have non-X-ray named property access"
    );

    browser.test.notifyPass("contentScriptXrays");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/*/file_sample.html"],
          js: ["content_script.js"],
        },
      ],
    },

    files: {
      "content_script.js": contentScript,
    },
  });

  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.awaitFinish("contentScriptXrays");

  await contentPage.close();
  await extension.unload();
});

add_task(async function test_dom_constructor_and_legacy_factory() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/*/file_sample.html"],
          js: ["xray_test.js"],
        },
      ],
      // Manifest v2 has its own XMLHttpRequest defined on the global,
      // which doesn't match the expectation below.
      manifest_version: 3,
    },

    files: {
      "xray_test.js"() {
        // The properties of the content window are inherit by the global,
        // but can be shadowed by globals of the sandbox itself.
        // Double-check that the sandbox does not have its own globals.
        browser.test.assertFalse(
          Object.hasOwn(globalThis, "XMLHttpRequest"),
          "XMLHttpRequest is not a sandbox global"
        );
        browser.test.assertFalse(
          Object.hasOwn(globalThis, "URL"),
          "URL is not a sandbox global"
        );
        browser.test.assertFalse(
          Object.hasOwn(globalThis, "Option"),
          "Option is not a sandbox global"
        );

        // Constructor function.
        browser.test.assertEq(
          XMLHttpRequest.name,
          "XMLHttpRequest",
          "XMLHttpRequest constructor's name property returns the correct value"
        );
        browser.test.assertEq(
          XMLHttpRequest.length,
          0,
          "XMLHttpRequest constructor's length property returns the correct value"
        );

        // Constructor function with the length being non-zero.
        browser.test.assertEq(
          URL.name,
          "URL",
          "URL constructor's name property returns the correct value"
        );
        browser.test.assertEq(
          URL.length,
          1,
          "URL constructor's length property returns the correct value"
        );

        // Legacy factory function.
        browser.test.assertEq(
          Option.name,
          "Option",
          "Options function's name property returns the correct value"
        );
        browser.test.assertEq(
          Option.length,
          0,
          "Options function's length property returns the correct value"
        );

        // Use eval in order to emulate the modification on the content side.
        // eslint-disable-next-line no-eval
        window.eval(`
Object.defineProperty(XMLHttpRequest, "name", { value: "A" });
Object.defineProperty(XMLHttpRequest, "length", { value: 9 });
Object.defineProperty(URL, "name", { value: "B" });
Object.defineProperty(URL, "length", { value: 8 });
Object.defineProperty(Option, "name", { value: "C" });
Object.defineProperty(Option, "length", { value: 7 });
`);

        // Modifications on the content side are visible to the content itself.
        // Use eval in order to emulate the access on the content side.
        browser.test.assertEq(
          // eslint-disable-next-line no-eval
          window.eval(`XMLHttpRequest.name`),
          "A",
          "XMLHttpRequest constructor's name property returns the modified value in content"
        );
        browser.test.assertEq(
          // eslint-disable-next-line no-eval
          window.eval(`XMLHttpRequest.length`),
          9,
          "XMLHttpRequest constructor's length property returns the modified value in content"
        );
        browser.test.assertEq(
          // eslint-disable-next-line no-eval
          window.eval(`URL.name`),
          "B",
          "URL constructor's name property returns the modified value in content"
        );
        browser.test.assertEq(
          // eslint-disable-next-line no-eval
          window.eval(`URL.length`),
          8,
          "URL constructor's length property returns the modified value in content"
        );
        browser.test.assertEq(
          // eslint-disable-next-line no-eval
          window.eval(`Option.name`),
          "C",
          "Option function's name property returns the modified value in content"
        );
        browser.test.assertEq(
          // eslint-disable-next-line no-eval
          window.eval(`Option.length`),
          7,
          "Option function's length property returns the modified value in content"
        );

        // Xray can see the original value.
        browser.test.assertEq(
          XMLHttpRequest.name,
          "XMLHttpRequest",
          "XMLHttpRequest constructor's name property returns the unmodified value"
        );
        browser.test.assertEq(
          XMLHttpRequest.length,
          0,
          "XMLHttpRequest constructor's length property returns the unmodified value"
        );
        browser.test.assertEq(
          URL.name,
          "URL",
          "URL constructor's name property returns the unmodified value"
        );
        browser.test.assertEq(
          URL.length,
          1,
          "URL constructor's length property returns the unmodified value"
        );
        browser.test.assertEq(
          Option.name,
          "Option",
          "Option function's name property returns the unmodified value"
        );
        browser.test.assertEq(
          Option.length,
          0,
          "Option function's length property returns the unmodified value"
        );

        browser.test.sendMessage("finished");
      },
    },
  });

  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.awaitMessage("finished");

  await contentPage.close();
  await extension.unload();
});
