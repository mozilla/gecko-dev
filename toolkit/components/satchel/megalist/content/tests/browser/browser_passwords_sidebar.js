/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SUPPORT_URL, PREFERENCES_URL } = ChromeUtils.importESModule(
  "resource://gre/modules/megalist/aggregator/datasources/LoginDataSource.sys.mjs"
);

const EXPECTED_PASSWORD_CARD_VALUES = [
  {
    "login-origin-field": { value: "example1.com" },
    "login-username-field": { value: "bob" },
    "login-password-field": { value: "••••••••" },
  },
  {
    "login-origin-field": { value: "example2.com" },
    "login-username-field": { value: "sally" },
    "login-password-field": { value: "••••••••" },
  },
  {
    "login-origin-field": { value: "example3.com" },
    "login-username-field": { value: "ned" },
    "login-password-field": { value: "••••••••" },
  },
];

function checkPasswordCardFields(megalist) {
  const list = megalist.querySelector(".passwords-list");
  const cards = list.querySelectorAll("password-card");

  for (let i = 0; i < EXPECTED_PASSWORD_CARD_VALUES.length; i++) {
    const card = cards[i];
    const expectedCard = EXPECTED_PASSWORD_CARD_VALUES[i];

    for (let selector of Object.keys(expectedCard)) {
      let actualValue = card.shadowRoot.querySelector(selector).value;
      const expectedValue = expectedCard[selector].value;

      if (selector === "login-password-field") {
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
      ["browser.megalist.enabled", true],
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
