/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global toggleMenuItem, TREE_FILTERS_MENU_ID */

addA11YPanelTask(
  `Test that Accessibility panel "All issues" audit doesn't return erroneous results`,
  "https://example.com/browser/devtools/client/accessibility/test/browser/test_audit_all.html",
  async env => {
    const { doc, toolbox } = env;
    info("Reload to have a clean state");
    // This is needed to reproduce the issue from Bug 1929891
    await reloadBrowser();

    info(`Select the "All issues" item to run all audits at once`);
    await toggleMenuItem(doc, toolbox.doc, TREE_FILTERS_MENU_ID, 1);

    info("Check that there's only 1 element in the tree");
    await checkTreeState(doc, [
      {
        role: "canvas",
        name: `""text label`,
        badges: ["text label"],
        level: 1,
        selected: true,
      },
    ]);
  }
);
