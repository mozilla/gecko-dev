/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserTestUtils: "resource://testing-common/BrowserTestUtils.sys.mjs",
});

/**
 * TabGroupTestUtils providers helpers for working with tab groups
 * in browser chrome tests.
 *
 * @class
 */
export const TabGroupTestUtils = {
  /**
   * Removes a tab group, along with its tabs. Resolves when the tab group
   * is gone.
   *
   * @param {MozTabbrowserTabGroup} group group to be removed
   * @returns {Promise<void>}
   */
  async removeTabGroup(group) {
    if (!group.parentNode) {
      // group was already removed
      return;
    }
    let removePromise = lazy.BrowserTestUtils.waitForEvent(
      group,
      "TabGroupRemoved"
    );
    await group.ownerGlobal.gBrowser.removeTabGroup(group, { animate: false });
    await removePromise;
  },
};
