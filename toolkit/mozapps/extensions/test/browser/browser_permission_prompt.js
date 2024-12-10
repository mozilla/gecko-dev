/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/*
 * Test Permission Popup for Sideloaded Extensions.
 */
const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
const ADDON_ID = "addon1@test.mozilla.org";
const CUSTOM_THEME_ID = "theme1@test.mozilla.org";
const DEFAULT_THEME_ID = "default-theme@mozilla.org";

ChromeUtils.defineESModuleGetters(this, {
  PERMISSION_L10N: "resource://gre/modules/ExtensionPermissionMessages.sys.mjs",
});

AddonTestUtils.initMochitest(this);

function assertDisabledSideloadedExtensionElement(managerWindow, addonElement) {
  const doc = addonElement.ownerDocument;
  const toggleDisabled = addonElement.querySelector(
    '[action="toggle-disabled"]'
  );
  is(
    doc.l10n.getAttributes(toggleDisabled).id,
    "extension-enable-addon-button-label",
    "Addon toggle-disabled action has the enable label"
  );
  ok(!toggleDisabled.checked, "toggle-disable isn't checked");
}

function assertEnabledSideloadedExtensionElement(managerWindow, addonElement) {
  const doc = addonElement.ownerDocument;
  const toggleDisabled = addonElement.querySelector(
    '[action="toggle-disabled"]'
  );
  is(
    doc.l10n.getAttributes(toggleDisabled).id,
    "extension-enable-addon-button-label",
    "Addon toggle-disabled action has the enable label"
  );
  ok(!toggleDisabled.checked, "toggle-disable isn't checked");
}

function clickEnableExtension(addonElement) {
  addonElement.querySelector('[action="toggle-disabled"]').click();
}

// Test for bug 1647931
// Install a theme, enable it and then enable the default theme again
add_task(async function test_theme_enable() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["xpinstall.signatures.required", false],
      ["extensions.autoDisableScopes", 15],
    ],
  });

  let theme = {
    manifest: {
      browser_specific_settings: { gecko: { id: CUSTOM_THEME_ID } },
      name: "Theme 1",
      theme: {
        colors: {
          frame: "#000000",
          tab_background_text: "#ffffff",
        },
      },
    },
  };

  let xpi = AddonTestUtils.createTempWebExtensionFile(theme);
  await AddonTestUtils.manuallyInstall(xpi);

  let changePromise = new Promise(resolve =>
    ExtensionsUI.once("change", resolve)
  );
  ExtensionsUI._checkForSideloaded();
  await changePromise;

  // enable fresh installed theme
  let manager = await open_manager("addons://list/theme");
  let customTheme = getAddonCard(manager, CUSTOM_THEME_ID);
  clickEnableExtension(customTheme);

  // enable default theme again
  let defaultTheme = getAddonCard(manager, DEFAULT_THEME_ID);
  clickEnableExtension(defaultTheme);

  let addon = await AddonManager.getAddonByID(CUSTOM_THEME_ID);
  await close_manager(manager);
  await addon.uninstall();
});

// Loading extension by sideloading method
add_task(async function test_sideloaded_extension_permissions_prompt() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["xpinstall.signatures.required", false],
      ["extensions.autoDisableScopes", 15],
    ],
  });

  let options = {
    manifest: {
      browser_specific_settings: { gecko: { id: ADDON_ID } },
      name: "Test 1",
      permissions: ["history", "https://*/*"],
      icons: { 64: "foo-icon.png" },
    },
  };

  let xpi = AddonTestUtils.createTempWebExtensionFile(options);
  await AddonTestUtils.manuallyInstall(xpi);

  let changePromise = new Promise(resolve =>
    ExtensionsUI.once("change", resolve)
  );
  ExtensionsUI._checkForSideloaded();
  await changePromise;

  // Test click event on permission cancel option.
  let manager = await open_manager("addons://list/extension");
  let addon = getAddonCard(manager, ADDON_ID);

  Assert.notEqual(addon, null, "Found sideloaded addon in about:addons");

  assertDisabledSideloadedExtensionElement(manager, addon);

  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  clickEnableExtension(addon);
  let panel = await popupPromise;

  ok(PopupNotifications.isPanelOpen, "Permission popup should be visible");
  panel.secondaryButton.click();
  ok(
    !PopupNotifications.isPanelOpen,
    "Permission popup should be closed / closing"
  );

  addon = await AddonManager.getAddonByID(ADDON_ID);
  ok(
    !addon.seen,
    "Seen flag should remain false after permissions are refused"
  );

  // Test click event on permission accept option.
  addon = getAddonCard(manager, ADDON_ID);
  Assert.notEqual(addon, null, "Found sideloaded addon in about:addons");

  assertEnabledSideloadedExtensionElement(manager, addon);

  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  clickEnableExtension(addon);
  panel = await popupPromise;

  ok(PopupNotifications.isPanelOpen, "Permission popup should be visible");

  let notificationPromise = acceptAppMenuNotificationWhenShown(
    "addon-installed",
    ADDON_ID
  );

  panel.button.click();
  ok(
    !PopupNotifications.isPanelOpen,
    "Permission popup should be closed / closing"
  );
  await notificationPromise;

  addon = await AddonManager.getAddonByID(ADDON_ID);
  ok(addon.seen, "Seen flag should be true after permissions are accepted");

  ok(!PopupNotifications.isPanelOpen, "Permission popup should not be visible");

  await close_manager(manager);
  await addon.uninstall();
});

add_task(async function testInstallDialogShowsFullDomainsList() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // These are both expected to be the default, but we are setting
      // them explicitly to make sure this test task is always running
      // with the prefs set with these values even if we would be
      // rolling back the pref value temporarily.
      ["extensions.ui.installDialogFullDomains", true],
      ["extensions.ui.postInstallPrivateBrowsingCheckbox", false],
    ],
  });
  // Sanity check.
  ok(
    ExtensionsUI.SHOW_FULL_DOMAINS_LIST,
    "Expect SHOW_FULL_DOMAINS_LIST to be enabled"
  );
  ok(
    !ExtensionsUI.POSTINSTALL_PRIVATEBROWSING_CHECKBOX,
    "Expect POSTINSTALL_PRIVATEBROWSING_CHECKBOX to be disabled"
  );

  const createTestExtensionXPI = ({
    id,
    domainsListLength = 0,
    permissions = [],
    incognito = "spanning",
  }) =>
    AddonTestUtils.createTempWebExtensionFile({
      manifest: {
        // Set the generated id as a name to make it easier to recognize the test case
        // from dialog screenshots (e.g. in the screenshot captured when the test hits
        // a failure).
        name: id,
        version: "1.0",
        browser_specific_settings: {
          gecko: { id },
        },
        incognito,
        permissions: permissions.concat(
          new Array(domainsListLength).fill("examplehost").map((v, i) => {
            return `*://${v}${i}.com/*`;
          })
        ),
      },
    });

  const LONG_DOMAIN_NAME = `averylongdomainname.${new Array(40)
    .fill("x")
    .join("")}.com`;

  const assertPermsElVisibility = (popupContentEl, noIncognitoCheckbox) => {
    // We expect the host permissions entry to be the only entry to be shown
    // if the incognito checkbox isn't expected to be visible for the test
    // extension (because the test extension doesn't request any other
    // permission and each test case is executed with and without opting-out
    // of the private browsing access).
    Assert.equal(
      BrowserTestUtils.isHidden(popupContentEl.permsListEl),
      noIncognitoCheckbox,
      `Expect the permissions list element to be ${
        noIncognitoCheckbox ? "hidden" : "visible"
      }`
    );
    Assert.equal(
      BrowserTestUtils.isVisible(popupContentEl.permsSingleEl),
      noIncognitoCheckbox,
      `Expect the single permission element to be ${
        noIncognitoCheckbox ? "visible" : "hidden"
      }`
    );
  };

  const assertNoDomainsList = popupContentEl => {
    const domainsListEl = popupContentEl.querySelector(
      ".webext-perm-domains-list"
    );
    Assert.ok(!domainsListEl, "Expect no domain list element to be found");
  };

  const TEST_CASES = [
    {
      msg: "Test install extension with no host permissions",
      id: "no-domains",
      domainsListLength: 0,
      verifyDialog(popupContentEl, noIncognitoCheckbox) {
        assertNoDomainsList(popupContentEl);
        Assert.ok(
          BrowserTestUtils.isHidden(popupContentEl.permsListEl),
          `Expect the permissions list element to be hidden`
        );
        Assert.equal(
          BrowserTestUtils.isHidden(popupContentEl.permsSingleEl),
          noIncognitoCheckbox,
          `Expect the permissions list element to be ${
            noIncognitoCheckbox ? "hidden" : "visible"
          }`
        );
      },
    },
    {
      msg: "Test install extension with one domain listed in host permissions",
      id: "one-domain",
      domainsListLength: 1,
      verifyDialog(popupContentEl, noIncognitoCheckbox) {
        assertPermsElVisibility(popupContentEl, noIncognitoCheckbox);
        assertNoDomainsList(popupContentEl);
        const hostPermStringEl = noIncognitoCheckbox
          ? popupContentEl.permsSingleEl
          : popupContentEl.permsListEl.querySelector("li.webext-perm-granted");
        Assert.ok(
          hostPermStringEl,
          "Expect one granted permission string element"
        );
        Assert.equal(
          hostPermStringEl.textContent,
          PERMISSION_L10N.formatValueSync(
            "webext-perms-host-description-one-domain",
            {
              domain: "examplehost0.com",
            }
          ),
          "Got the expected host permission string on extension with only one granted domain"
        );
        Assert.ok(
          BrowserTestUtils.isVisible(hostPermStringEl),
          "Expect the host permission string to be visible"
        );
      },
    },
    {
      msg: "Test install extension with less than 6 domains listed in host permissions",
      id: "few-domains",
      domainsListLength: 5,
      verifyDialog(popupContentEl, noIncognitoCheckbox) {
        assertPermsElVisibility(popupContentEl, noIncognitoCheckbox);
        const domainsListEl = noIncognitoCheckbox
          ? popupContentEl.permsSingleEl.querySelector(
              ".webext-perm-domains-list"
            )
          : popupContentEl.permsListEl.querySelector(
              ".webext-perm-domains-list"
            );

        Assert.ok(
          domainsListEl,
          "Expect domains list element to be found inside the permission list element"
        );
        Assert.ok(
          BrowserTestUtils.isVisible(domainsListEl),
          "Expect the domains list element to be visible"
        );
        Assert.equal(
          domainsListEl.scrollTopMax,
          0,
          "Expect domains list to not be scrollable (chromeOnly scrollTopMax set to 0)"
        );
        // The permission string associated to XUL label element can be reached as labelEl.value.
        Assert.equal(
          domainsListEl.previousElementSibling.value,
          PERMISSION_L10N.formatValueSync(
            "webext-perms-host-description-multiple-domains",
            {
              domainCount: this.domainsListLength,
            }
          ),
          `Got the expected host permission string on extension with ${this.domainsListLength} granted domain`
        );
        Assert.deepEqual(
          Array.from(domainsListEl.querySelectorAll("li")).map(
            el => el.textContent
          ),
          new Array(this.domainsListLength)
            .fill("examplehost")
            .map((v, i) => `${v}${i}.com`),
          "Got the expected domains listed in the domains list element"
        );
      },
    },
    {
      msg: "Test install extension with many domains listed in host permissions",
      id: "many-domains",
      domainsListLength: 20,
      verifyDialog(popupContentEl, noIncognitoCheckbox) {
        assertPermsElVisibility(popupContentEl, noIncognitoCheckbox);
        const domainsListEl = noIncognitoCheckbox
          ? popupContentEl.permsSingleEl.querySelector(
              ".webext-perm-domains-list"
            )
          : popupContentEl.permsListEl.querySelector(
              ".webext-perm-domains-list"
            );

        Assert.ok(
          domainsListEl,
          "Expect domains list element to be found inside the permission list element"
        );
        Assert.ok(
          BrowserTestUtils.isVisible(domainsListEl),
          "Expect the domains list element to be visible"
        );
        Assert.greater(
          domainsListEl.scrollTopMax,
          domainsListEl.clientHeight,
          "Expect domains list to be scrollable (chromeOnly scrollTopMax greater than clientHeight)"
        );
        // The permission string associated to XUL label element can be reached as labelEl.value.
        Assert.equal(
          domainsListEl.previousElementSibling.value,
          PERMISSION_L10N.formatValueSync(
            "webext-perms-host-description-multiple-domains",
            {
              domainCount: this.domainsListLength,
            }
          ),
          `Got the expected host permission string on extension with ${this.domainsListLength} granted domain`
        );
        Assert.deepEqual(
          Array.from(domainsListEl.querySelectorAll("li")).map(
            el => el.textContent
          ),
          new Array(this.domainsListLength)
            .fill("examplehost")
            .map((v, i) => `${v}${i}.com`),
          "Got the expected domains listed in the domains list element"
        );
      },
    },
    {
      msg: "Test text wrapping on a single long domain name",
      id: "one-long-domain",
      domainsListLength: 0,
      permissions: [`*://${LONG_DOMAIN_NAME}/*`],
      verifyDialog(popupContentEl, noIncognitoCheckbox) {
        assertPermsElVisibility(popupContentEl, noIncognitoCheckbox);
        const hostPermStringEl = noIncognitoCheckbox
          ? popupContentEl.permsSingleEl
          : popupContentEl.permsListEl.querySelector("li.webext-perm-granted");
        Assert.equal(
          hostPermStringEl.childNodes[0].nodeType,
          hostPermStringEl.TEXT_NODE,
          "Expect to have host permission element child to be a text node"
        );
        Assert.equal(
          hostPermStringEl.childNodes[0].nodeValue,
          PERMISSION_L10N.formatValueSync(
            "webext-perms-host-description-one-domain",
            {
              domain: LONG_DOMAIN_NAME,
            }
          ),
          "Got the expected host permission string set as the nextNode value"
        );
        Assert.deepEqual(
          // Calling getBoxQuads on the text node is expected to be returning
          // one DOMQuad instance for each line the text node has been broken
          // into (e.g. 3 DOMQuad instances when the long domain name forces
          // the text node to be broken over 3 lines).
          //
          // This check is asserting that none of the lines the text node has
          // been broken into has a width larger than the width of the parent
          // element.
          //
          // NOTE: this assertion is expected to hit a failure if .webext-perm-granted
          // or .addon-webext-perm-single-entry elements are missing the overflow-wrap
          // CSS rule.
          hostPermStringEl.childNodes[0]
            .getBoxQuads()
            .map(quad => quad.getBounds().width)
            .filter(width => {
              return width > hostPermStringEl.getBoundingClientRect().width;
            }),
          [],
          "The host permission text node should NOT overflow the parent element"
        );
      },
    },
    {
      msg: "Test text wrapping on long domain name in domains list",
      id: "one-long-domain-in-domains-list",
      domainsListLength: 10,
      permissions: [`*://${LONG_DOMAIN_NAME}/*`],
      verifyDialog(popupContentEl, noIncognitoCheckbox) {
        assertPermsElVisibility(popupContentEl, noIncognitoCheckbox);
        const domainsListEl = noIncognitoCheckbox
          ? popupContentEl.permsSingleEl.querySelector(
              ".webext-perm-domains-list"
            )
          : popupContentEl.permsListEl.querySelector(
              ".webext-perm-domains-list"
            );
        Assert.ok(
          domainsListEl,
          "Expect domains list element to be found inside the permission list element"
        );
        Assert.ok(
          BrowserTestUtils.isVisible(domainsListEl),
          "Expect the domains list element to be visible"
        );
        Assert.equal(
          domainsListEl.firstElementChild.childNodes[0].nodeType,
          domainsListEl.TEXT_NODE,
          "Found text node for the long domain name item part of the domain list item"
        );
        Assert.equal(
          domainsListEl.firstElementChild.childNodes[0].nodeValue,
          LONG_DOMAIN_NAME,
          "Got the expected domain name set on the first domain list item"
        );
        Assert.deepEqual(
          // This check is asserting that none of the lines the text node has
          // been broken into has a width larger than the width of the parent
          // element.
          //
          // NOTE: this assertion is expected to hit a failure if .webext-perm-domains-list
          // list items elements are overflowing (e.g. if it is not inheriting the
          // overflow-wrap CSS rule from its ascending notes and doesn't have one set on
          // its own).
          domainsListEl.firstElementChild.childNodes[0]
            .getBoxQuads()
            .map(quad => quad.getBounds().width)
            .filter(width => {
              return (
                width >
                domainsListEl.firstElementChild.getBoundingClientRect().width
              );
            }),
          [],
          "The domain name text node should NOT overflow the parent element"
        );
      },
    },
    {
      msg: "Test wildcard subdomains shown as single host permission",
      id: "with-wildcard-subdomains",
      domainsListLength: 0,
      permissions: ["*://*.example.com/*", "*://example.com/*"],
      verifyDialog(popupContentEl, noIncognitoCheckbox) {
        assertPermsElVisibility(popupContentEl, noIncognitoCheckbox);
        const hostPermStringEl = noIncognitoCheckbox
          ? popupContentEl.permsSingleEl
          : popupContentEl.permsListEl.querySelector("li.webext-perm-granted");
        Assert.equal(
          hostPermStringEl.textContent,
          PERMISSION_L10N.formatValueSync(
            "webext-perms-host-description-one-domain",
            {
              domain: "example.com",
            }
          ),
          "Expected *.example.com and example.com host permissions to be reported as a single domain permission string"
        );
      },
    },
    {
      msg: "Test wildcard subdomains in domains list",
      id: "with-wildcard-subdomains",
      domainsListLength: 0,
      permissions: [
        "*://*.example.com/*",
        "*://example.com/*`",
        "*://*.example.org/*",
        "*://example.org/*",
      ],
      verifyDialog(popupContentEl, noIncognitoCheckbox) {
        assertPermsElVisibility(popupContentEl, noIncognitoCheckbox);
        assertPermsElVisibility(popupContentEl, noIncognitoCheckbox);
        const domainsListEl = noIncognitoCheckbox
          ? popupContentEl.permsSingleEl.querySelector(
              ".webext-perm-domains-list"
            )
          : popupContentEl.permsListEl.querySelector(
              ".webext-perm-domains-list"
            );
        Assert.ok(
          domainsListEl,
          "Expect domains list element to be found inside the permission list element"
        );
        Assert.ok(
          BrowserTestUtils.isVisible(domainsListEl),
          "Expect the domains list element to be visible"
        );
        // Expect the domains list to only include 2 domains and the host permissions string
        // to reflect that as well.
        Assert.deepEqual(
          Array.from(domainsListEl.querySelectorAll("li")).map(
            el => el.textContent
          ),
          ["example.com", "example.org"],
          "Got the expected domains listed in the domains list element"
        );
        Assert.equal(
          domainsListEl.previousElementSibling.value,
          PERMISSION_L10N.formatValueSync(
            "webext-perms-host-description-multiple-domains",
            {
              domainCount: 2,
            }
          ),
          "Got the expected host permission string on extension with 2 granted domain"
        );
      },
    },
  ];

  for (const testCase of TEST_CASES) {
    // Repeat each test without and without the private browsing checkbox
    // (to test the dialog when the host permissions is expected to be part of a list
    // or to be the only permissions listed in the dialog).
    for (const incognito of ["spanning", "not_allowed"]) {
      const noIncognitoCheckbox = incognito === "not_allowed";
      info(
        `${testCase.msg} ${
          noIncognitoCheckbox ? "and no other permissions" : ""
        }`
      );
      const xpi = createTestExtensionXPI({
        ...testCase,
        id: `${testCase.id}-with-incognito-${incognito}@test-ext`,
        incognito,
      });

      await BrowserTestUtils.withNewTab("about:blank", async () => {
        const dialogPromise = promisePopupNotificationShown(
          "addon-webext-permissions"
        );

        gURLBar.value = xpi.path;
        gURLBar.focus();
        EventUtils.synthesizeKey("KEY_Enter");
        const popupContentEl = await dialogPromise;

        testCase.verifyDialog(popupContentEl, noIncognitoCheckbox);

        let popupHiddenPromise = BrowserTestUtils.waitForEvent(
          window.PopupNotifications.panel,
          "popuphidden"
        );
        // hide the panel (this simulates the user dismissing it)
        popupContentEl.closest("panel").hidePopup();
        await popupHiddenPromise;
      });
    }
  }

  await SpecialPowers.popPrefEnv();
});
