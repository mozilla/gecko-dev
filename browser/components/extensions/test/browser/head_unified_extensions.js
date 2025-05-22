/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported assertExtensionsButtonHidden,
            assertExtensionsButtonVisible,
            assertExtensionsButtonTelemetry,
            resetExtensionsButtonTelemetry,
            clickUnifiedExtensionsItem,
            closeCustomizationUI,
            closeExtensionsPanel,
            createExtensions,
            ensureMaximizedWindow,
            ensureWindowInnerDimensions,
            getBlockKey
            getMessageBars,
            getUnifiedExtensionsItem,
            loadBlocklistRawData,
            openCustomizationUI,
            openExtensionsPanel,
            openUnifiedExtensionsContextMenu,
            promiseSetToolbarVisibility
*/

const assertExtensionsButtonHidden = (win = window) => {
  const { button } = win.gUnifiedExtensions;
  ok(BrowserTestUtils.isHidden(button), "Extensions button should be hidden");
  if (win.document.querySelector("#nav-bar[unifiedextensionsbuttonshown]")) {
    ok(false, "Found unexpected unifiedextensionsbuttonshown attribute");
  }
};

const assertExtensionsButtonVisible = (win = window) => {
  const { button } = win.gUnifiedExtensions;
  ok(BrowserTestUtils.isVisible(button), "Extensions button should be visible");
  if (!win.document.querySelector("#nav-bar[unifiedextensionsbuttonshown]")) {
    ok(false, "Missing unifiedextensionsbuttonshown attribute");
  }
};

const getListView = (win = window) => {
  const { panel } = win.gUnifiedExtensions;
  ok(panel, "expected panel to be created");
  return panel.querySelector("#unified-extensions-view");
};

const openExtensionsPanel = async (win = window) => {
  const { button } = win.gUnifiedExtensions;
  assertExtensionsButtonVisible(win);

  const listView = getListView(win);
  ok(listView, "expected list view");

  const viewShown = BrowserTestUtils.waitForEvent(listView, "ViewShown");
  button.click();
  await viewShown;
};

const closeExtensionsPanel = async (win = window) => {
  const { button } = win.gUnifiedExtensions;
  assertExtensionsButtonVisible(win);

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

const openCustomizationUI = async (win = window) => {
  const customizationReady = BrowserTestUtils.waitForEvent(
    win.gNavToolbox,
    "customizationready"
  );
  win.gCustomizeMode.enter();
  await customizationReady;
  ok(
    win.CustomizationHandler.isCustomizing(),
    "expected customizing mode to be enabled"
  );
};

const closeCustomizationUI = async (win = window) => {
  const afterCustomization = BrowserTestUtils.waitForEvent(
    win.gNavToolbox,
    "aftercustomization"
  );
  win.gCustomizeMode.exit();
  await afterCustomization;
  ok(
    !win.CustomizationHandler.isCustomizing(),
    "expected customizing mode to be disabled"
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

// All labels of the extensions_button.temporarily_unhidden labeled counter.
// There is no API to get all labels (bug 1930572), so list them manually:
const extensionsButtonTemporarilyUnhiddenLabels = [
  "customize",
  "addon_install_doorhanger",
  "extension_controlled_setting",
  "extension_permission_prompt",
  "extensions_panel_showing",
  "extension_browser_action_popup",
  "attention_blocklist",
  "attention_permission_denied",
];
const extensionsButtonTemporarilyUnhiddenLastValues = {};

// Reset the extensions_button.temporarily_unhidden counters. Must be called by
// the test before assertExtensionsButtonTelemetry, to ensure that there is no
// unrelated state.
function resetExtensionsButtonTelemetry() {
  // It is not possible to reset the labels of an individual Glean metric, only
  // all of it can be reset with Services.fog.testResetFOG(). But we don't want
  // to rely on that, because the telemetry tests are inserted in other tests
  // that happen to already trigger the scenario where this telemetry is
  // relevant, and we want to minimize the impact on the observable behavior in
  // these tests. So, instead of resetting, we just store the last known state
  // of the counters.
  for (const k of extensionsButtonTemporarilyUnhiddenLabels) {
    extensionsButtonTemporarilyUnhiddenLastValues[k] =
      Glean.extensionsButton.temporarilyUnhidden[k].testGetValue() ?? 0;
  }
}

// Checks the values of all extensions_button.temporarily_unhidden counters.
function assertExtensionsButtonTelemetry(expectations) {
  // We also need to check __other__ to ensure that we did not inadvertently
  // misspell a label somewhere (bug 1905345).

  const expectedKeys = new Set(Object.keys(expectations));
  const actual = {};
  for (const k of extensionsButtonTemporarilyUnhiddenLabels) {
    let value = Glean.extensionsButton.temporarilyUnhidden[k].testGetValue();
    value ??= 0; // testGetValue() returns null if never recorded before.
    // Compute delta compared to last call of resetExtensionsButtonTelemetry():
    value -= extensionsButtonTemporarilyUnhiddenLastValues[k];
    // Record if in expectations (even if 0). Record if value is non-zero, to
    // make sure that the caller in the test is notified of unexpected values.
    if (expectedKeys.has(k) || value) {
      actual[k] = value;
    }
    expectedKeys.delete(k);
  }
  if (expectedKeys.size !== 0) {
    // Sanity check: check that expectations are not misspelled / missing.
    Assert.ok(false, `Unrecognized expectations: ${Array.from(expectedKeys)}`);
  }
  Assert.deepEqual(
    actual,
    expectations,
    "extensions_button.temporarily_unhidden has expected counters on its labels"
  );

  // Glean happily increments counters for labels that are not explicitly
  // listed in metrics.yaml. Make sure that we don't have unexpected labels.
  // Currently, the only way to check that is via __other__ (bug 1905345).
  let o = Glean.extensionsButton.temporarilyUnhidden.__other__.testGetValue();
  if (o !== null) {
    Assert.equal(o, null, "All static labels should be accounted for");
  }
}
