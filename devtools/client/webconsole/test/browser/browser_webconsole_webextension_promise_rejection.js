/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that an uncaught promise rejection from a content script
// is reported to the tabs' webconsole.

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-blank.html";

add_task(async function () {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: [TEST_URI],
          js: ["content-script.js"],
        },
      ],
    },

    files: {
      "content-script.js": function () {
        Promise.reject("abc");
      },
    },
  });

  await extension.startup();

  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(() => findErrorMessage(hud, "uncaught exception: abc"));

  // Open the debugger with the content script setting turned on in order
  // to be able to show the content script target in the console evaluation context
  await pushPref("devtools.debugger.show-content-scripts", true);
  await hud.toolbox.selectTool("jsdebugger");

  const evaluationContextSelectorButton = hud.ui.outputNode.querySelector(
    ".webconsole-evaluation-selector-button"
  );
  ok(
    evaluationContextSelectorButton,
    "The evaluation context selector is visible"
  );

  info("Assert the content of the evaluation context menu");
  // Note that the context menu is in the top level chrome document (toolbox.xhtml)
  // instead of webconsole.xhtml.
  const labels = hud.toolbox.doc.querySelectorAll(
    "#webconsole-console-evaluation-context-selector-menu-list li .label"
  );
  is(labels[0].textContent, "Top");
  ok(!labels[0].closest(".menu-item").classList.contains("indented"));
  is(labels[1].textContent, "Generated extension");
  ok(labels[1].closest(".menu-item").classList.contains("indented"));

  await extension.unload();
});
