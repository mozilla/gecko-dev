"use strict";

const { JSObjectsTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/JSObjectsTestUtils.sys.mjs"
);
JSObjectsTestUtils.init(this);

// Note that XPCShellContentUtils will already be initialized by JSObjectsTestUtils
const { XPCShellContentUtils } = ChromeUtils.importESModule(
  "resource://testing-common/XPCShellContentUtils.sys.mjs"
);

const EXPECTED_VALUES_FILE = "test_javascript_logging.snapshot.mjs";

add_task(async function () {
  // Create a content page in order to better simulate a real world page
  const contentPage = await XPCShellContentUtils.loadContentPage(
    "http://example.com/"
  );

  await JSObjectsTestUtils.runTest(EXPECTED_VALUES_FILE, async function (arg) {
    // Because the test page runs in a content process, we have to execute most of the test logic via `spawn`
    return contentPage.spawn([arg], async ({ context, expression }) => {
      const { CONTEXTS } = ChromeUtils.importESModule(
        "resource://testing-common/AllJavascriptTypes.mjs"
      );
      const { JSTracer } = ChromeUtils.importESModule(
        "resource://devtools/server/tracer/tracer.sys.mjs"
      );
      const systemPrincipal =
        Services.scriptSecurityManager.getSystemPrincipal();
      const chromeSandbox = Cu.Sandbox(systemPrincipal);

      let ref;
      try {
        if (context == CONTEXTS.CHROME) {
          ref = Cu.evalInSandbox(
            expression,
            chromeSandbox,
            null,
            "test sandbox"
          );
        } else {
          ref = this.content.eval(expression);
        }
      } catch (e) {
        ref = e;
      }

      const str = JSTracer.objectToString(ref);

      // Silence any async rejection
      if (ref instanceof this.content.Promise) {
        // eslint-disable-next-line max-nested-callbacks
        ref.catch(function () {});
      }

      return str;
    });
  });

  info("Close content page");
  await contentPage.close();
});
