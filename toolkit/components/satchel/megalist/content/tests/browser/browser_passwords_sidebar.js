/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SUPPORT_URL, PREFERENCES_URL } = ChromeUtils.importESModule(
  "resource://gre/modules/megalist/aggregator/datasources/LoginDataSource.sys.mjs"
);

const { OSKeyStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/OSKeyStoreTestUtils.sys.mjs"
);

const EXPECTED_PASSWORD_CARD_VALUES = [
  {
    originLine: { value: "example1.com" },
    usernameLine: { value: "bob" },
    passwordLine: { value: "••••••••" },
  },
  {
    originLine: { value: "example2.com" },
    usernameLine: { value: "sally" },
    passwordLine: { value: "••••••••" },
  },
  {
    originLine: { value: "example3.com" },
    usernameLine: { value: "ned" },
    passwordLine: { value: "••••••••" },
  },
];

function checkPasswordCardFields(megalist) {
  const list = megalist.querySelector(".passwords-list");
  const cards = list.querySelectorAll("password-card");

  for (let i = 0; i < EXPECTED_PASSWORD_CARD_VALUES.length; i++) {
    const card = cards[i];
    const expectedCard = EXPECTED_PASSWORD_CARD_VALUES[i];
    for (let selector of Object.keys(expectedCard)) {
      let actualValue = card[selector].value;
      const expectedValue = expectedCard[selector].value;

      if (selector === "passwordLine") {
        // Skip since we don't expose password value
        continue;
      }

      is(
        actualValue,
        expectedValue,
        `Login entry ${i}: Rendered field value for ${selector} is correct`
      );
    }
  }
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contextual-password-manager.enabled", true],
      ["signon.rememberSignons", true],
    ],
  });
  registerCleanupFunction(LoginTestUtils.clearData);
});

add_task(async function test_passwords_sidebar() {
  await addMockPasswords();
  const megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  info("Check correct initial login info is rendered.");
  checkPasswordCardFields(megalist);

  LoginTestUtils.clearData();
  info("Closing the sidebar");
  SidebarController.hide();
});

// Select the button within the <panel-item> element, as that is the target we want to click.
// Triggering a click event directly on the <panel-item> would fail a11y tests because
// <panel-item>  has a default role="presentation," which cannot be overridden.
const getShadowBtn = (menu, selector) =>
  menu.querySelector(selector).shadowRoot.querySelector("button");

add_task(async function test_login_line_commands() {
  await addLocalOriginLogin();
  const passwordsSidebar = await openPasswordsSidebar();
  const list = passwordsSidebar.querySelector(".passwords-list");
  const card = list.querySelector("password-card");
  const expectedPasswordCard = {
    originLine: { value: "about:preferences#privacy" },
    usernameLine: { value: "john" },
    passwordLine: { value: "pass4" },
  };
  const selectors = Object.keys(expectedPasswordCard);

  for (const selector of selectors) {
    let loginLine = card[selector];
    if (selector === "originLine") {
      const browserLoadedPromise = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        expectedPasswordCard[selector].value
      );
      info(`click on ${selector}`);
      loginLine.click();
      await browserLoadedPromise;
      ok(true, "origin url loaded");
    } else if (selector === "usernameLine") {
      await SimpleTest.promiseClipboardChange(
        expectedPasswordCard[selector].value,
        () => {
          info(`click on ${selector}`);
          loginLine.click();
        }
      );
    } else if (
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin() &&
      selector === "passwordLine"
    ) {
      const loginLine = card[selector].loginLine;
      /* Once we pass the prompt message and pref to #verifyUser in MegalistViewModel,
       * we will have to do something like this:
       * let reauthObserved = Promise.resolve();
       * if (OSKeyStore.canReauth()) {
       *		reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
       * }
       * ...
       * await reauthObserved;
       */
      await SimpleTest.promiseClipboardChange(
        expectedPasswordCard[selector].value,
        () => {
          info(`click on ${selector}`);
          loginLine.click();
        }
      );
    }
  }

  BrowserTestUtils.addTab(gBrowser, "about:blank");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  SidebarController.hide();
});

add_task(async function test_passwords_menu_external_links() {
  const passwordsSidebar = await openPasswordsSidebar();
  const menu = passwordsSidebar.querySelector("panel-list");
  const menuButton = passwordsSidebar.querySelector("#more-options-menubutton");

  menuButton.click();
  await BrowserTestUtils.waitForEvent(menu, "shown");

  const preferencesTabPromise = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    PREFERENCES_URL
  );

  getShadowBtn(menu, "[action='open-preferences']").click();
  await preferencesTabPromise;
  ok(true, "passwords settings in preferences opened.");

  menuButton.click();
  await BrowserTestUtils.waitForEvent(menu, "shown");
  const helpTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, SUPPORT_URL);

  getShadowBtn(menu, "[action='open-help']").click();
  const helpTab = await helpTabPromise;
  ok(true, "support link opened.");

  BrowserTestUtils.removeTab(helpTab);
  // We need this since removing gBrowser.selectedTab (this is the tab that has about:preferences)
  // without a fallback causes an error. Leaving it causes a leak when running in chaos mode.
  // It seems that our testing framework is smart enough to cleanup about:blank pages.
  BrowserTestUtils.addTab(gBrowser, "about:blank");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  SidebarController.hide();
});

async function waitForMigrationWizard() {
  let wizardReadyPromise = BrowserTestUtils.waitForEvent(
    window,
    "MigrationWizard:Ready"
  );
  await BrowserTestUtils.waitForLocationChange(
    gBrowser,
    "about:preferences#general"
  );
  return wizardReadyPromise;
}

add_task(async function test_passwords_menu_import_from_browser() {
  const passwordsSidebar = await openPasswordsSidebar();
  const menu = passwordsSidebar.querySelector("panel-list");
  const menuButton = passwordsSidebar.querySelector("#more-options-menubutton");

  menuButton.click();
  await BrowserTestUtils.waitForEvent(menu, "shown");

  const wizardReadyPromise = waitForMigrationWizard();
  getShadowBtn(menu, "[action='import-from-browser']").click();
  const wizard = await wizardReadyPromise;
  ok(wizard, "migration wizard opened");
  BrowserTestUtils.addTab(gBrowser, "about:blank");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  SidebarController.hide();
});
