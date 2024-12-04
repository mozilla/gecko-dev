/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(4);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);

loadTestSubscript("head_unified_extensions.js");

const assertExtensionMessagebar = ({
  addonId,
  isCUIWidget,
  expectInArea = CustomizableUI.AREA_ADDONS,
  expectOverflowedWidget = false,
  expectMessageBarType = "warning",
  expectVisible,
  expectFluentId,
  expectFluentArgs,
}) => {
  let messagebar;
  if (isCUIWidget) {
    let widget =
      AppUiTestInternals.getBrowserActionWidget(addonId)?.forWindow(window);
    ok(widget, `Found CUI widget for ${addonId}`);
    messagebar = widget.node.querySelector("moz-message-bar");
    ok(messagebar, `Found messagebar for ${addonId} CUI widget`);
    const placement = CustomizableUI.getPlacementOfWidget(widget.id);
    Assert.equal(
      placement.area,
      expectInArea,
      "CUI widget placement should have the expected CUI area"
    );
    Assert.equal(
      widget.overflowed,
      expectOverflowedWidget,
      `Expect CUI widget for ${addonId} to ${
        expectOverflowedWidget ? "be" : "NOT be"
      } overflowed`
    );
  } else {
    let item = getUnifiedExtensionsItem(addonId);
    Assert.ok(item, `Expect extensions panel entry for ${addonId}`);
    messagebar = item.querySelector("moz-message-bar");
    ok(messagebar, `Found messagebar for ${addonId} unified-extensions-item`);
  }

  Assert.equal(
    BrowserTestUtils.isVisible(messagebar),
    expectVisible,
    `Expect ${addonId} messagebar to be ${expectVisible ? "visible" : "hidden"}`
  );

  if (expectVisible) {
    Assert.equal(
      messagebar.getAttribute("type"),
      expectMessageBarType,
      `${addonId} messagebar should have the expected messagebar type`
    );
    Assert.deepEqual(
      {
        messageL10nId: messagebar.messageL10nId,
        messageL10nArgs: messagebar.messageL10nArgs,
      },
      {
        messageL10nId: expectFluentId,
        messageL10nArgs: expectFluentArgs,
      },
      `${addonId} messagebar should have the expected fluent id and fluent args`
    );
    const mbText = messagebar.shadowRoot.querySelector(
      ".text-content .message"
    ).textContent;
    Assert.ok(!!mbText.length, "Expect messagebar message to not be empty");
    Assert.ok(
      mbText.includes(WebExtensionPolicy.getByID(addonId).name),
      "Expect messagebar message to include the extension name"
    );
  }
};

const prepareSoftBlockEnabledExtension = async addonId => {
  const addon = await AddonManager.getAddonByID(addonId);
  await loadBlocklistRawData({
    softblocked: [addon],
  });
  const promiseReady = awaitEvent("ready", addon.id);
  await addon.enable();
  await promiseReady;
  Assert.equal(
    addon.blocklistState,
    Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
    "Expect addon blocklistState to be STATE_SOFTBLOCKED"
  );
  const policy = WebExtensionPolicy.getByID(addon.id);
  Assert.equal(
    policy.extension.blocklistState,
    addon.blocklistState,
    "Expect extension.blocklistState to match addon.blocklistState"
  );
  Assert.ok(
    policy.extension.isSoftBlocked,
    "Expect extension.isSoftBlocked to be true"
  );
  return addon;
};

add_task(async function test_softblocked_item_messagebar() {
  const [extWithAction, extNoAction] = createExtensions([
    {
      name: "ExtWithActionName",
      browser_specific_settings: { gecko: { id: "@extWithAction" } },
      browser_action: {},
    },
    {
      name: "ExtNoActionName",
      browser_specific_settings: { gecko: { id: "@extNoAction" } },
    },
  ]);

  await extWithAction.startup();
  await extNoAction.startup();

  info("Verify extension panel items messagebars are initially hidden");
  await openExtensionsPanel();
  for (const ext of [extWithAction, extNoAction]) {
    assertExtensionMessagebar({
      addonId: ext.id,
      isCUIWidget: ext.id !== "@extNoAction",
      expectVisible: false,
    });
  }
  await closeExtensionsPanel();

  info("Verify messagebar from CUI widget pinned in the navbar is hidden");
  const widgetID = AppUiTestInternals.getBrowserActionWidgetId(
    extWithAction.id
  );
  CustomizableUI.addWidgetToArea(widgetID, CustomizableUI.AREA_NAVBAR);
  assertExtensionMessagebar({
    addonId: extWithAction.id,
    isCUIWidget: true,
    expectInArea: CustomizableUI.AREA_NAVBAR,
    expectVisible: false,
  });
  await CustomizableUI.reset();

  info(
    "Verify extension panel items messagebars on re-enabled softblocked extensions with browserAction"
  );

  let addon = await prepareSoftBlockEnabledExtension(extWithAction.id);
  await openExtensionsPanel();
  info("Verify panel item messagebar is visible in the panel");
  assertExtensionMessagebar({
    addonId: addon.id,
    isCUIWidget: true,
    expectVisible: true,
    expectFluentId: "unified-extensions-item-messagebar-softblocked",
    expectFluentArgs: { extensionName: addon.name },
  });
  await closeExtensionsPanel();

  info(
    "Verify panel item messagebar is hidden when the same CUI widget is pinned in the navbar"
  );
  CustomizableUI.addWidgetToArea(widgetID, CustomizableUI.AREA_NAVBAR);
  assertExtensionMessagebar({
    addonId: addon.id,
    isCUIWidget: true,
    expectInArea: CustomizableUI.AREA_NAVBAR,
    expectVisible: false,
  });

  info(
    "Verify panel item messagebar is visible on CUI widget pinned on an hidden area shown in the panel"
  );
  CustomizableUI.addWidgetToArea(widgetID, CustomizableUI.AREA_BOOKMARKS);
  const bookmarksToolbar = document.getElementById(
    CustomizableUI.AREA_BOOKMARKS
  );
  await promiseSetToolbarVisibility(bookmarksToolbar, false);
  await openExtensionsPanel();
  assertExtensionMessagebar({
    addonId: addon.id,
    isCUIWidget: true,
    expectInArea: CustomizableUI.AREA_BOOKMARKS,
    expectOverflowedWidget: true,
    expectVisible: true,
    expectFluentId: "unified-extensions-item-messagebar-softblocked",
    expectFluentArgs: { extensionName: addon.name },
  });
  await closeExtensionsPanel();

  info("Verify panel item messagebar is hidden when the softblock is lifted");
  await loadBlocklistRawData({
    softblocked: [],
  });
  await openExtensionsPanel();
  assertExtensionMessagebar({
    addonId: addon.id,
    isCUIWidget: true,
    expectInArea: CustomizableUI.AREA_BOOKMARKS,
    expectOverflowedWidget: true,
    expectVisible: false,
  });
  await closeExtensionsPanel();

  await CustomizableUI.reset();

  info(
    "Verify extension panel items messagebars on re-enabled softblocked extensions without browserAction"
  );

  addon = await prepareSoftBlockEnabledExtension(extNoAction.id);
  await openExtensionsPanel();
  info("Verify panel item messagebar is visible in the panel");
  assertExtensionMessagebar({
    addonId: addon.id,
    isCUIWidget: false,
    expectVisible: true,
    expectFluentId: "unified-extensions-item-messagebar-softblocked",
    expectFluentArgs: { extensionName: addon.name },
  });
  await closeExtensionsPanel();

  info("Verify panel item messagebar is hidden when the softblock is lifted");
  await loadBlocklistRawData({
    softblocked: [],
  });
  await openExtensionsPanel();
  assertExtensionMessagebar({
    addonId: addon.id,
    isCUIWidget: false,
    expectVisible: false,
  });
  await closeExtensionsPanel();

  await extNoAction.unload();
  await extWithAction.unload();
});
