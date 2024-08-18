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

const openPasswordsSidebar = async () => {
  info("Open Passwords sidebar");
  await SidebarController.show("viewMegalistSidebar");
  const sidebar = document.getElementById("sidebar");
  const passwordsSidebar =
    sidebar.contentDocument.querySelector("megalist-alpha").shadowRoot;
  return passwordsSidebar;
};

function checkPasswordCardFields(passwordsSidebar) {
  const list = passwordsSidebar.querySelector(".passwords-list");
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
  const passwordsSidebar = await openPasswordsSidebar();

  info("Check that records are rendered");
  await BrowserTestUtils.waitForMutationCondition(
    passwordsSidebar,
    { childList: true, subtree: true },
    () => {
      const passwordsList = passwordsSidebar.querySelector(".passwords-list");
      return passwordsList?.querySelectorAll("password-card").length === 3;
    }
  );
  ok(true, "3 password cards are rendered.");

  info("Check correct initial login info is rendered.");
  checkPasswordCardFields(passwordsSidebar);

  LoginTestUtils.clearData();
  info("Closing the sidebar");
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

  menu.querySelector("[action='open-preferences']").click();
  await preferencesTabPromise;
  ok(true, "passwords settings in preferences opened.");

  menuButton.click();
  await BrowserTestUtils.waitForEvent(menu, "shown");
  const helpTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, SUPPORT_URL);

  menu.querySelector("[action='open-help']").click();
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
