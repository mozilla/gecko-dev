const TEST_URL =
  "https://example.com/browser/dom/tests/browser/page_test_report_missing_child_module.html";

const EXPECTED_MESSAGE = "Loading failed for the module with source ";

function waitForError(expectedBadModule) {
  return new Promise(resolve => {
    const listener = {
      QueryInterface: ChromeUtils.generateQI(["nsIConsoleListener"]),
      observe(message) {
        if (
          message.message.includes(expectedBadModule) &&
          message.message.includes(EXPECTED_MESSAGE)
        ) {
          message.QueryInterface(Ci.nsIScriptError);
          Services.console.unregisterListener(listener);
          resolve(message);
        }
      },
    };
    Services.console.registerListener(listener);
  });
}

add_task(async function () {
  // We need the console listeners in place BEFORE we load the test page.
  const childLoadPromise = waitForError(
    "/intentionally-non-existent-child-module.mjs"
  );
  const grandchildLoadPromise = waitForError(
    "/intentionally-non-existent-grandchild-module.mjs"
  );

  await BrowserTestUtils.withNewTab(TEST_URL, async function () {
    const childLoadError = await childLoadPromise;
    Assert.stringContains(
      childLoadError.sourceName,
      "file_test_report_missing_child_module_parent.mjs",
      "A failure to load a top-level module's child should be attributed to the top-level module"
    );

    const grandchildLoadError = await grandchildLoadPromise;
    Assert.stringContains(
      grandchildLoadError.sourceName,
      "file_test_report_missing_child_module_parent_02.mjs",
      "A failure to load a non-top-level module A's child B should be attributed to A"
    );
  });
});
