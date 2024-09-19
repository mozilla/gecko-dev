/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function testOpenBrowserWindow() {
  let win = OpenBrowserWindow();
  Assert.ok(
    !PrivateBrowsingUtils.isWindowPrivate(win),
    "OpenBrowserWindow() should open a normal window"
  );
  await BrowserTestUtils.closeWindow(win);

  win = OpenBrowserWindow({ private: true });
  Assert.ok(
    PrivateBrowsingUtils.isWindowPrivate(win),
    "OpenBrowserWindow({private: true}) should open a private window"
  );
  await BrowserTestUtils.closeWindow(win);

  win = OpenBrowserWindow({ private: false });
  Assert.ok(
    !PrivateBrowsingUtils.isWindowPrivate(win),
    "OpenBrowserWindow({private: false}) should open a normal window"
  );
  await BrowserTestUtils.closeWindow(win);

  // In permanent browsing mode.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.privatebrowsing.autostart", true]],
  });

  win = OpenBrowserWindow();
  Assert.ok(
    PrivateBrowsingUtils.isWindowPrivate(win),
    "OpenBrowserWindow() in PBM should open a private window"
  );
  await BrowserTestUtils.closeWindow(win);

  win = OpenBrowserWindow({ private: true });
  Assert.ok(
    PrivateBrowsingUtils.isWindowPrivate(win),
    "OpenBrowserWindow({private: true}) in PBM should open a private window"
  );
  await BrowserTestUtils.closeWindow(win);

  win = OpenBrowserWindow({ private: false });
  Assert.ok(
    PrivateBrowsingUtils.isWindowPrivate(win),
    "OpenBrowserWindow({private: false}) in PBM should open a private window"
  );
  await BrowserTestUtils.closeWindow(win);

  await SpecialPowers.popPrefEnv();
});

/**
 * Check that the "new window" menu items have the expected properties.
 *
 * @param {Element} newWindowItem - The "new window" item to check.
 * @param {Element} privateWindowItem - The "new private window" item to check.
 * @param {Object} expect - The expected properties.
 * @param {boolean} expect.privateVisible - Whether we expect the private item
 *   to be visible or not.
 * @param {string} expect.newWindowL10nId - The expected string ID used by the
 *   "new window" item.
 * @param {string} expect.privateWindowL10nId - The expected string ID used by
 *   the "new private window" item.
 * @param {boolean} [useIsVisible=true] - Whether to test the "true" visibility
 *   of the item. Otherwise only the "hidden" attribute is checked.
 */
function assertMenuItems(
  newWindowItem,
  privateWindowItem,
  expect,
  useIsVisible = true
) {
  Assert.ok(newWindowItem);
  Assert.ok(privateWindowItem);

  if (useIsVisible) {
    Assert.ok(
      BrowserTestUtils.isVisible(newWindowItem),
      "New window item should be visible"
    );
  } else {
    // The application menu is not accessible on macOS, just check the hidden
    // attribute.
    Assert.ok(!newWindowItem.hidden, "New window item should be visible");
  }

  Assert.equal(
    newWindowItem.getAttribute("key"),
    "key_newNavigator",
    "New window item should use the same key"
  );
  Assert.equal(
    newWindowItem.getAttribute("data-l10n-id"),
    expect.newWindowL10nId
  );

  if (!expect.privateVisible) {
    if (useIsVisible) {
      Assert.ok(
        BrowserTestUtils.isHidden(privateWindowItem),
        "Private window item should be hidden"
      );
    } else {
      Assert.ok(
        privateWindowItem.hidden,
        "Private window item should be hidden"
      );
    }
    // Don't check attributes since hidden.
  } else {
    if (useIsVisible) {
      Assert.ok(
        BrowserTestUtils.isVisible(privateWindowItem),
        "Private window item should be visible"
      );
    } else {
      Assert.ok(
        !privateWindowItem.hidden,
        "Private window item should be visible"
      );
    }
    Assert.equal(
      privateWindowItem.getAttribute("key"),
      "key_privatebrowsing",
      "Private window item should use the same key"
    );
    Assert.equal(
      privateWindowItem.getAttribute("data-l10n-id"),
      expect.privateWindowL10nId
    );
  }
}

/**
 * Check that a window has the expected "new window" items in the "File" and app
 * menus.
 *
 * @param {Window} win - The window to check.
 * @param {boolean} expectBoth - Whether we expect the window to contain both
 *   "new window" and "new private window" as separate controls.
 */
async function checkWindowMenus(win, expectBoth) {
  // Check the File menu.
  assertMenuItems(
    win.document.getElementById("menu_newNavigator"),
    win.document.getElementById("menu_newPrivateWindow"),
    {
      privateVisible: expectBoth,
      // If in permanent private browsing, expect the new window item to use the
      // "New private window" string.
      newWindowL10nId: expectBoth
        ? "menu-file-new-window"
        : "menu-file-new-private-window",
      privateWindowL10nId: "menu-file-new-private-window",
    },
    // The file menu is difficult to open cross-platform, so we do not open it
    // for this test.
    false
  );

  // Open the app menu.
  let appMenuButton = win.document.getElementById("PanelUI-menu-button");
  let appMenu = win.document.getElementById("appMenu-popup");
  let menuShown = BrowserTestUtils.waitForEvent(appMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(appMenuButton, {}, win);
  await menuShown;

  // Check the app menu.
  assertMenuItems(
    win.document.getElementById("appMenu-new-window-button2"),
    win.document.getElementById("appMenu-new-private-window-button2"),
    {
      privateVisible: expectBoth,
      // If in permanent private browsing, expect the new window item to use the
      // "New private window" string.
      newWindowL10nId: expectBoth
        ? "appmenuitem-new-window"
        : "appmenuitem-new-private-window",
      privateWindowL10nId: "appmenuitem-new-private-window",
    }
  );

  appMenu.hidePopup();
}

add_task(async function testNewWindowMenuItems() {
  // In non-private window, expect both menu items.
  let win = await BrowserTestUtils.openNewBrowserWindow({
    private: false,
  });
  await checkWindowMenus(win, true);
  Assert.equal(win.gBrowser.currentURI.spec, "about:blank");
  await BrowserTestUtils.closeWindow(win);

  // In non-permanent private window, still expect both menu items.
  win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await checkWindowMenus(win, true);
  Assert.equal(win.gBrowser.currentURI.spec, "about:privatebrowsing");
  await BrowserTestUtils.closeWindow(win);

  // In permanent private browsing, expect only one menu item.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.privatebrowsing.autostart", true]],
  });

  win = await BrowserTestUtils.openNewBrowserWindow();
  await checkWindowMenus(win, false);
  Assert.equal(win.gBrowser.currentURI.spec, "about:blank");
  await BrowserTestUtils.closeWindow(win);

  await SpecialPowers.popPrefEnv();
});
