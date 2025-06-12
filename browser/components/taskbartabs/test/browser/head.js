/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  TaskbarTabsRegistry:
    "resource:///modules/taskbartabs/TaskbarTabsRegistry.sys.mjs",
  TaskbarTabsWindowManager:
    "resource:///modules/taskbartabs/TaskbarTabsWindowManager.sys.mjs",
});

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["network.dns.localDomains", "www.test.com"]],
  });
});

/**
 * Creates a web app window with the given tab,
 * then returns the window object for testing.
 *
 * @param {Tab} aTab
 *        The tab that the web app should open with
 * @returns {Promise}
 *        The web app window object.
 */
async function openTaskbarTabWindow(aTab = null) {
  const url = Services.io.newURI("https://www.test.com");
  const userContextId = 0;

  const registry = new TaskbarTabsRegistry();
  const taskbarTab = registry.findOrCreateTaskbarTab(url, userContextId);
  const windowManager = new TaskbarTabsWindowManager();

  const windowPromise = BrowserTestUtils.waitForNewWindow();

  if (aTab) {
    windowManager.replaceTabWithWindow(taskbarTab, aTab);
  } else {
    windowManager.openWindow(taskbarTab);
  }

  return await windowPromise;
}
