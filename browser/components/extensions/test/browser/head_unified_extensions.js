/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported clickUnifiedExtensionsItem,
            closeExtensionsPanel,
            createExtensions,
            ensureMaximizedWindow,
            ensureWindowInnerDimensions,
            getBlockKey
            getMessageBars,
            getUnifiedExtensionsItem,
            loadBlocklistRawData,
            openExtensionsPanel,
            openUnifiedExtensionsContextMenu,
            promiseSetToolbarVisibility
*/

const getListView = (win = window) => {
  const { panel } = win.gUnifiedExtensions;
  ok(panel, "expected panel to be created");
  return panel.querySelector("#unified-extensions-view");
};

const openExtensionsPanel = async (win = window) => {
  const { button } = win.gUnifiedExtensions;
  ok(button, "expected button");

  const listView = getListView(win);
  ok(listView, "expected list view");

  const viewShown = BrowserTestUtils.waitForEvent(listView, "ViewShown");
  button.click();
  await viewShown;
};

const closeExtensionsPanel = async (win = window) => {
  const { button } = win.gUnifiedExtensions;
  ok(button, "expected button");

  const hidden = BrowserTestUtils.waitForEvent(
    win.gUnifiedExtensions.panel,
    "popuphidden",
    true
  );
  button.click();
  await hidden;
};

const getUnifiedExtensionsItem = (extensionId, win = window) => {
  const view = getListView(win);

  // First try to find a CUI widget, otherwise a custom element when the
  // extension does not have a browser action.
  return (
    view.querySelector(`toolbaritem[data-extensionid="${extensionId}"]`) ||
    view.querySelector(`unified-extensions-item[extension-id="${extensionId}"]`)
  );
};

const openUnifiedExtensionsContextMenu = async (extensionId, win = window) => {
  const item = getUnifiedExtensionsItem(extensionId, win);
  ok(item, `expected item for extensionId=${extensionId}`);
  const button = item.querySelector(".unified-extensions-item-menu-button");
  ok(button, "expected menu button");
  // Make sure the button is visible before clicking on it (below) since the
  // list of extensions can have a scrollbar (when there are many extensions
  // and/or the window is small-ish).
  button.scrollIntoView({ block: "center" });

  const menu = win.document.getElementById("unified-extensions-context-menu");
  ok(menu, "expected menu");

  const shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  // Use primary button click to open the context menu.
  EventUtils.synthesizeMouseAtCenter(button, {}, win);
  await shown;

  return menu;
};

const clickUnifiedExtensionsItem = async (
  win,
  extensionId,
  forceEnableButton = false
) => {
  // The panel should be closed automatically when we click an extension item.
  await openExtensionsPanel(win);

  const item = getUnifiedExtensionsItem(extensionId, win);
  ok(item, `expected item for ${extensionId}`);

  // The action button should be disabled when users aren't supposed to click
  // on it but it might still be useful to re-enable it for testing purposes.
  if (forceEnableButton) {
    let actionButton = item.querySelector(
      ".unified-extensions-item-action-button"
    );
    actionButton.disabled = false;
    ok(!actionButton.disabled, "action button was force-enabled");
  }

  // Similar to `openUnifiedExtensionsContextMenu()`, we make sure the item is
  // visible before clicking on it to prevent intermittents.
  item.scrollIntoView({ block: "center" });

  const popupHidden = BrowserTestUtils.waitForEvent(
    win.document,
    "popuphidden",
    true
  );
  EventUtils.synthesizeMouseAtCenter(item, {}, win);
  await popupHidden;
};

const createExtensions = (
  arrayOfManifestData,
  { useAddonManager = true, incognitoOverride, files } = {}
) => {
  return arrayOfManifestData.map(manifestData =>
    ExtensionTestUtils.loadExtension({
      manifest: {
        name: "default-extension-name",
        ...manifestData,
      },
      useAddonManager: useAddonManager ? "temporary" : undefined,
      incognitoOverride,
      files,
    })
  );
};

const ensureStableDimensions = async win => {
  let lastOuterWidth = win.outerWidth;
  let lastOuterHeight = win.outerHeight;
  let sameSizeTimes = 0;
  await TestUtils.waitForCondition(() => {
    const isSameSize =
      win.outerWidth === lastOuterWidth && win.outerHeight === lastOuterHeight;
    if (!isSameSize) {
      lastOuterWidth = win.outerWidth;
      lastOuterHeight = win.outerHeight;
    }
    sameSizeTimes = isSameSize ? sameSizeTimes + 1 : 0;
    return sameSizeTimes === 10;
  }, "Wait for the chrome window size to settle");
};

const _ensureSizeMode = (win, expectedMode, change) => {
  info(`ensuring sizeMode ${expectedMode}`);
  if (win.windowState == expectedMode) {
    info(`already the right mode`);
    return;
  }

  return new Promise(resolve => {
    win.addEventListener("sizemodechange", function listener() {
      info(`sizeMode changed to ${win.windowState}`);
      if (win.windowState == expectedMode) {
        win.removeEventListener("sizemodechange", listener);
        resolve();
      }
    });
    change();
  });
};

/**
 * Given a window, this test helper resizes it so that the window takes most of
 * the available screen size (unless the window is already maximized).
 */
const ensureMaximizedWindow = win => {
  return _ensureSizeMode(win, win.STATE_MAXIMIZED, () => win.maximize());
};

const ensureWindowInnerDimensions = async (win, w, h) => {
  // restore() first if needed, because some Linux compositors (older Mutter)
  // will not honor the size request otherwise.
  await _ensureSizeMode(win, win.STATE_NORMAL, () => win.restore());

  await ensureStableDimensions(win);

  let diffX = w ? w - win.innerWidth : 0;
  let diffY = h ? h - win.innerHeight : 0;
  info(`Resizing to ${w} (${diffX}), ${h} (${diffY})`);
  if (diffX || diffY) {
    await new Promise(resolve => {
      win.addEventListener("resize", resolve, { once: true });
      win.resizeBy(diffX, diffY);
    });
    info(`Got window resize: ${win.innerWidth}x${win.innerHeight}`);
  }
};

const promiseSetToolbarVisibility = (toolbar, visible) => {
  const visibilityChanged = BrowserTestUtils.waitForMutationCondition(
    toolbar,
    { attributeFilter: ["collapsed"] },
    () => toolbar.collapsed != visible
  );
  setToolbarVisibility(toolbar, visible, undefined, false);
  return visibilityChanged;
};

const getMessageBars = (win = window) => {
  const { panel } = win.gUnifiedExtensions;
  return panel.querySelectorAll(
    "#unified-extensions-messages-container > moz-message-bar"
  );
};

const getBlockKey = ({ id, version }) => {
  if (!id || !version) {
    // Throw an error if the resulting block key would not be a valid one.
    throw new Error(
      "getBlockKey requires id and version to be defined and non-empty"
    );
  }
  return `${id}:${version}`;
};

const loadBlocklistRawData = async stash => {
  const { AddonTestUtils } = ChromeUtils.importESModule(
    "resource://testing-common/AddonTestUtils.sys.mjs"
  );
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      {
        stash: {
          blocked: stash.blocked?.map(getBlockKey) ?? [],
          softblocked: stash.softblocked?.map(getBlockKey) ?? [],
          unblocked: stash.unblocked?.map(getBlockKey) ?? [],
        },
        stash_time: 0,
      },
    ],
  });
  let needsCleanupBlocklist = true;
  const cleanupBlocklist = async () => {
    if (!needsCleanupBlocklist) {
      return;
    }
    await AddonTestUtils.loadBlocklistRawData({
      extensionsMLBF: [],
    });
    needsCleanupBlocklist = false;
  };
  registerCleanupFunction(cleanupBlocklist);
  return cleanupBlocklist;
};
