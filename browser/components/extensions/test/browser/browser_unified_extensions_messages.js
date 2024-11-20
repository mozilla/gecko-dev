/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(4);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);

loadTestSubscript("head_unified_extensions.js");

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

const installTestExtension = async (id, name) => {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      name,
      browser_specific_settings: { gecko: { id } },
    },
  });

  await extension.startup();
  const addon = await AddonManager.getAddonByID(id);
  ok(addon, `Expect AddonWrapper instance for ${id} to be found`);
  return { addon, extension };
};

const verifyBlocklistAttentionDot = expected => {
  const unifiedButton = document.querySelector("#unified-extensions-button");
  Assert.equal(
    unifiedButton.hasAttribute("attention"),
    expected,
    "Got the expected attention attribute on the extensions button"
  );
};

const verifyBlocklistAttentionMessageBar = (message, expected) => {
  Assert.equal(
    message.getAttribute("type"),
    expected.type,
    "Got expected messagebar type"
  );
  Assert.ok(
    message.hasAttribute("dismissable"),
    "Expect message to be dismissable"
  );

  Assert.deepEqual(
    message.ownerDocument.l10n.getAttributes(message),
    expected.fluentAttributes,
    "Got expected fluent attributes set on the messagebar element"
  );

  const linkToAboutAddons = message.querySelector(
    "a.unified-extensions-link-to-aboutaddons"
  );
  Assert.equal(
    message.ownerDocument.l10n.getAttributes(linkToAboutAddons)?.id,
    "unified-extensions-mb-about-addons-link",
    "Got expected fluent id on the slotted link to aboutaddons"
  );
};

const verifyExtensionButtonFluentId = async expectedFluentId => {
  const unifiedButton = document.querySelector("#unified-extensions-button");
  Assert.equal(
    document.l10n.getAttributes(unifiedButton)?.id,
    expectedFluentId,
    "Got the expected fluent id on the extensions button"
  );
};

const verifyQuarantinedDomainsMessageBar = async message => {
  Assert.equal(
    message.getAttribute("type"),
    "warning",
    "expected warning message"
  );
  Assert.ok(
    !message.hasAttribute("dismissable"),
    "expected message to not be dismissable"
  );

  const supportLink = message.querySelector("a");
  Assert.equal(
    supportLink.getAttribute("support-page"),
    "quarantined-domains",
    "expected the correct support page ID"
  );
  // If we do not wait for the element to be translated
  // by fluent, we may hit intermittent failures when
  // the assertion that follows is hit before fluent
  // have actually set the aria-label on this element.
  await document.l10n.translateElements([supportLink]);

  Assert.equal(
    supportLink.getAttribute("aria-label"),
    "Learn more: Some extensions are not allowed",
    "expected the correct aria-labelledby value"
  );
};

add_task(async function test_quarantined_domain_message_disabled() {
  const quarantinedDomain = "example.org";
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.quarantinedDomains.enabled", false],
      ["extensions.quarantinedDomains.list", quarantinedDomain],
    ],
  });

  // Load an extension that will have access to all domains, including the
  // quarantined domain.
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["activeTab"],
      browser_action: {},
    },
  });
  await extension.startup();

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `https://${quarantinedDomain}/` },
    async () => {
      verifyExtensionButtonFluentId("unified-extensions-button");
      await openExtensionsPanel();
      Assert.equal(getMessageBars().length, 0, "expected no message");
      await closeExtensionsPanel();
    }
  );

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_quarantined_domain_message() {
  const quarantinedDomain = "example.org";
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.quarantinedDomains.enabled", true],
      ["extensions.quarantinedDomains.list", quarantinedDomain],
    ],
  });

  // Load an extension that will have access to all domains, including the
  // quarantined domain.
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["activeTab"],
      browser_action: {},
    },
  });
  await extension.startup();
  verifyExtensionButtonFluentId("unified-extensions-button");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `https://${quarantinedDomain}/` },
    async () => {
      verifyExtensionButtonFluentId("unified-extensions-button-quarantined");
      await openExtensionsPanel();

      const messages = getMessageBars();
      Assert.equal(messages.length, 1, "expected a message");

      const [message] = messages;
      await verifyQuarantinedDomainsMessageBar(message);

      await closeExtensionsPanel();
    }
  );

  // Navigating to a different tab/domain shouldn't show any message.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `http://mochi.test:8888/` },
    async () => {
      verifyExtensionButtonFluentId("unified-extensions-button");
      await openExtensionsPanel();
      Assert.equal(getMessageBars().length, 0, "expected no message");
      await closeExtensionsPanel();
    }
  );

  // Back to a quarantined domain, if we update the list, we expect the message
  // to be gone when we re-open the panel (and not before because we don't
  // listen to the pref currently).
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `https://${quarantinedDomain}/` },
    async () => {
      verifyExtensionButtonFluentId("unified-extensions-button-quarantined");
      await openExtensionsPanel();

      const messages = getMessageBars();
      Assert.equal(messages.length, 1, "expected a message");

      const [message] = messages;
      await verifyQuarantinedDomainsMessageBar(message);

      await closeExtensionsPanel();

      const unifiedButton = document.querySelector(
        "#unified-extensions-button"
      );
      const promiseFluentIdChanged = BrowserTestUtils.waitForMutationCondition(
        unifiedButton,
        { attributes: true, attributeFilter: ["data-l10n-id"] },
        () =>
          document.l10n.getAttributes(unifiedButton)?.id !==
          "unified-extensions-button-quarantined"
      );

      // Clear the list of quarantined domains.
      Services.prefs.setStringPref("extensions.quarantinedDomains.list", "");

      await openExtensionsPanel();
      Assert.equal(getMessageBars().length, 0, "expected no message");
      await closeExtensionsPanel();

      // Wait for the extension button fluent id change.
      await promiseFluentIdChanged;
      verifyExtensionButtonFluentId("unified-extensions-button");
    }
  );

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_quarantined_domain_message_learn_more_link() {
  const quarantinedDomain = "example.org";
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.quarantinedDomains.enabled", true],
      ["extensions.quarantinedDomains.list", quarantinedDomain],
    ],
  });

  // Load an extension that will have access to all domains, including the
  // quarantined domain.
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["activeTab"],
      browser_action: {},
    },
  });
  await extension.startup();

  const expectedSupportURL =
    Services.urlFormatter.formatURLPref("app.support.baseURL") +
    "quarantined-domains";

  // We expect the SUMO page to be open in a new tab and the panel to be closed
  // when the user clicks on the "learn more" link.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `https://${quarantinedDomain}/` },
    async () => {
      await openExtensionsPanel();
      const messages = getMessageBars();
      Assert.equal(messages.length, 1, "expected a message");

      const [message] = messages;
      await verifyQuarantinedDomainsMessageBar(message);

      const tabPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        expectedSupportURL
      );
      const hidden = BrowserTestUtils.waitForEvent(
        gUnifiedExtensions.panel,
        "popuphidden",
        true
      );
      message.querySelector("a").click();
      const [tab] = await Promise.all([tabPromise, hidden]);
      BrowserTestUtils.removeTab(tab);
    }
  );

  // Same as above but with keyboard navigation.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `https://${quarantinedDomain}/` },
    async () => {
      await openExtensionsPanel();
      const messages = getMessageBars();
      Assert.equal(messages.length, 1, "expected a message");

      const [message] = messages;
      await verifyQuarantinedDomainsMessageBar(message);

      const supportLink = message.querySelector("a");

      // Focus the "learn more" (support) link.
      const focused = BrowserTestUtils.waitForEvent(supportLink, "focus");
      EventUtils.synthesizeKey("VK_TAB", {});
      await focused;

      const tabPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        expectedSupportURL
      );
      const hidden = BrowserTestUtils.waitForEvent(
        gUnifiedExtensions.panel,
        "popuphidden",
        true
      );
      EventUtils.synthesizeKey("KEY_Enter", {});
      const [tab] = await Promise.all([tabPromise, hidden]);
      BrowserTestUtils.removeTab(tab);
    }
  );

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});

async function runBlockedExtensionsTestCase({ testSoftBlocks = false }) {
  const { addon: addon1, extension: ext1 } = await installTestExtension(
    "@ext1",
    "ExtName1"
  );
  const { addon: addon2, extension: ext2 } = await installTestExtension(
    "@ext2",
    "ExtName2"
  );

  verifyBlocklistAttentionDot(false);
  verifyExtensionButtonFluentId("unified-extensions-button");

  let promiseBlocklistAttentionUpdated = AddonTestUtils.promiseManagerEvent(
    "onBlocklistAttentionUpdated"
  );
  const cleanupBlocklist = await loadBlocklistRawData(
    testSoftBlocks ? { softblocked: [addon1] } : { blocked: [addon1] }
  );
  info("Wait for onBlocklistAttentionUpdated manager listener call");
  await promiseBlocklistAttentionUpdated;
  verifyBlocklistAttentionDot(true);
  verifyExtensionButtonFluentId("unified-extensions-button-blocklisted");

  info("Verify blocklist attention messagebar on a single hard-block");

  {
    await openExtensionsPanel();
    const messages = getMessageBars();
    Assert.equal(messages.length, 1, "expected a message");
    const [message] = messages;
    verifyBlocklistAttentionMessageBar(message, {
      type: testSoftBlocks ? "warning" : "error",
      fluentAttributes: {
        id: testSoftBlocks
          ? "unified-extensions-mb-blocklist-warning-single"
          : "unified-extensions-mb-blocklist-error-single",
        args: {
          extensionName: addon1.name,
          extensionsCount: 1,
        },
      },
    });

    info("Verify blocklist attention messagebar dismissed");
    promiseBlocklistAttentionUpdated = AddonTestUtils.promiseManagerEvent(
      "onBlocklistAttentionUpdated",
      () => !AddonManager.shouldShowBlocklistAttention()
    );
    message.dismiss();
    info(
      "Wait for onBlocklistAttentionUpdated manager listener calls to clear the attention"
    );

    Assert.deepEqual(
      {
        addon1_dismissed: addon1.blocklistAttentionDismissed,
        addon2_dismissed: addon2.blocklistAttentionDismissed,
      },
      {
        addon1_dismissed: true,
        addon2_dismissed: false,
      },
      "Expect only addon1.blocklistAttentionDismissed to be set to true"
    );
    verifyBlocklistAttentionDot(false);
    verifyExtensionButtonFluentId("unified-extensions-button");

    Assert.equal(
      getMessageBars().length,
      0,
      "expect messagebar to have been removed"
    );

    await closeExtensionsPanel();
  }

  // Un-dismiss addon1 to cover the slightly different messagebar expected when more than
  // one addon is hard blocked.
  addon1.blocklistAttentionDismissed = false;

  promiseBlocklistAttentionUpdated = AddonTestUtils.promiseManagerEvent(
    "onBlocklistAttentionUpdated"
  );
  await loadBlocklistRawData(
    testSoftBlocks
      ? { softblocked: [addon1, addon2] }
      : { blocked: [addon1, addon2] }
  );

  info("Wait for onBlocklistAttentionUpdated manager listener call");
  await promiseBlocklistAttentionUpdated;
  verifyBlocklistAttentionDot(true);
  verifyExtensionButtonFluentId("unified-extensions-button-blocklisted");

  info("Verify blocklist attention messagebar on multiple hard-blocks");

  {
    await openExtensionsPanel();
    let messages = getMessageBars();
    Assert.equal(messages.length, 1, "expected a message");

    // Re-open the panel and confirm there is still only one messagebar.
    await closeExtensionsPanel();
    await openExtensionsPanel();
    messages = getMessageBars();
    Assert.equal(
      messages.length,
      1,
      "expected only one message (after re-opening the panel)"
    );

    const [message] = messages;
    verifyBlocklistAttentionMessageBar(message, {
      type: testSoftBlocks ? "warning" : "error",
      fluentAttributes: {
        id: testSoftBlocks
          ? "unified-extensions-mb-blocklist-warning-multiple"
          : "unified-extensions-mb-blocklist-error-multiple",
        args: {
          extensionsCount: 2,
        },
      },
    });

    info("Verify blocklist attention messagebar dismissed");
    promiseBlocklistAttentionUpdated = AddonTestUtils.promiseManagerEvent(
      "onBlocklistAttentionUpdated",
      () => !AddonManager.shouldShowBlocklistAttention()
    );
    message.dismiss();
    info(
      "Wait for onBlocklistAttentionUpdated manager listener calls to clear the attention"
    );

    Assert.deepEqual(
      {
        addon1_dismissed: addon1.blocklistAttentionDismissed,
        addon2_dismissed: addon2.blocklistAttentionDismissed,
      },
      {
        addon1_dismissed: true,
        addon2_dismissed: true,
      },
      "Expect only addon1.blocklistAttentionDismissed to be set to true"
    );
    verifyBlocklistAttentionDot(false);
    verifyExtensionButtonFluentId("unified-extensions-button");
    Assert.equal(
      getMessageBars().length,
      0,
      "expect messagebar to have been removed"
    );

    await closeExtensionsPanel();
  }

  await cleanupBlocklist();
  await ext2.unload();
  await ext1.unload();
}

// This test task cover blocklist attention on single and multiple hard-blocks
// and expected behaviors on dismissing the blocklist messagebar.
add_task(async function test_blocklist_attention_on_hardblocks() {
  await runBlockedExtensionsTestCase({
    testSoftBlocks: false,
  });
});

// This test task cover blocklist attention on single and multiple soft-blocks
// and expected behaviors on dismissing the blocklist messagebar.
add_task(async function test_blocklist_attention_on_softblocks() {
  await runBlockedExtensionsTestCase({
    testSoftBlocks: true,
  });
});

// This additional test task covers:
// - messagebar type error when there are both soft and hard blocks
// - clicking on the "Go to extension settings" link expected to open an about:addons tab
add_task(async function test_blocklist_attention_on_soft_and_hardblocks() {
  // Select a non-empty tab, to make sure clicking of the "Go to extension settings"
  // link in the messagebar will open a new tab instead of replacing the current
  // empty tab.
  const newTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );
  const { addon: addon1, extension: ext1 } = await installTestExtension(
    "@ext1",
    "ExtName1"
  );
  const { addon: addon2, extension: ext2 } = await installTestExtension(
    "@ext2",
    "ExtName2"
  );

  verifyBlocklistAttentionDot(false);
  verifyExtensionButtonFluentId("unified-extensions-button");

  let promiseBlocklistAttentionUpdated = AddonTestUtils.promiseManagerEvent(
    "onBlocklistAttentionUpdated"
  );
  const cleanupBlocklist = await loadBlocklistRawData({
    softblocked: [addon1],
    blocked: [addon2],
  });
  info("Wait for onBlocklistAttentionUpdated manager listener call");
  await promiseBlocklistAttentionUpdated;
  verifyBlocklistAttentionDot(true);
  verifyExtensionButtonFluentId("unified-extensions-button-blocklisted");

  info("Verify blocklist attention messagebar on hard-block and soft-blocks");

  await openExtensionsPanel();
  const messages = getMessageBars();
  Assert.equal(messages.length, 1, "expected a message");
  const [message] = messages;
  verifyBlocklistAttentionMessageBar(message, {
    type: "error",
    fluentAttributes: {
      id: "unified-extensions-mb-blocklist-error-multiple",
      args: {
        extensionsCount: 2,
      },
    },
  });

  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");
  message.querySelector("a.unified-extensions-link-to-aboutaddons").click();
  // about:addons should load and go to the list of extensions
  const aboutAddonsTab = await tabPromise;
  is(
    aboutAddonsTab.linkedBrowser.currentURI.spec,
    "about:addons",
    "Browser is at about:addons"
  );
  info("Expect extensions list view to be loaded");
  await TestUtils.waitForCondition(() => {
    const aboutAddonsWin = aboutAddonsTab.linkedBrowser.contentWindow;
    return (
      aboutAddonsWin.gViewController?.currentViewId ===
      "addons://list/extension"
    );
  }, "Wait for extensions list view to have been loaded in the about:addons tab");

  Assert.equal(
    gBrowser.selectedTab,
    aboutAddonsTab,
    "Expect the about:addons tab to have been selected"
  );

  BrowserTestUtils.removeTab(aboutAddonsTab);
  BrowserTestUtils.removeTab(newTab);

  await cleanupBlocklist();
  await ext2.unload();
  await ext1.unload();
});

add_task(async function test_quarantined_and_blocklist_message() {
  const quarantinedDomain = "example.org";
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.quarantinedDomains.enabled", true],
      ["extensions.quarantinedDomains.list", quarantinedDomain],
    ],
  });

  // Load an extension that will have access to all domains, including the
  // quarantined domain.
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["activeTab"],
      browser_action: {},
    },
  });
  await extension.startup();

  const { addon: blockedaddon1, extension: blockedext1 } =
    await installTestExtension("@blocklisted-ext1", "BlocklisteExtName1");

  verifyExtensionButtonFluentId("unified-extensions-button");
  verifyBlocklistAttentionDot(false);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `https://${quarantinedDomain}/` },
    async () => {
      verifyExtensionButtonFluentId("unified-extensions-button-quarantined");
      verifyBlocklistAttentionDot(true);

      await openExtensionsPanel();
      let messages = getMessageBars();
      Assert.equal(messages.length, 1, "expected a message");
      verifyQuarantinedDomainsMessageBar(messages[0]);
      await closeExtensionsPanel();

      let promiseBlocklistAttentionUpdated = AddonTestUtils.promiseManagerEvent(
        "onBlocklistAttentionUpdated"
      );
      const cleanupBlocklist = await loadBlocklistRawData({
        blocked: [blockedaddon1],
      });

      info("Wait for onBlocklistAttentionUpdated manager listener call");
      await promiseBlocklistAttentionUpdated;
      verifyExtensionButtonFluentId("unified-extensions-button-blocklisted");
      verifyBlocklistAttentionDot(true);

      await openExtensionsPanel();
      messages = getMessageBars();
      Assert.equal(messages.length, 2, "expected a message");
      verifyBlocklistAttentionMessageBar(messages[0], {
        type: "error",
        fluentAttributes: {
          id: "unified-extensions-mb-blocklist-error-single",
          args: {
            extensionName: blockedaddon1.name,
            extensionsCount: 1,
          },
        },
      });
      verifyQuarantinedDomainsMessageBar(messages[1]);
      await closeExtensionsPanel();
      cleanupBlocklist();
    }
  );

  await blockedext1.unload();
  await extension.unload();
});
