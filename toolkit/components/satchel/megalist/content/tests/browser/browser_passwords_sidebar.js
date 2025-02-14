/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SUPPORT_URL, PREFERENCES_URL } = ChromeUtils.importESModule(
  "resource://gre/modules/megalist/aggregator/datasources/LoginDataSource.sys.mjs"
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

async function waitForPasswordConceal(passwordLine) {
  const concealedPromise = BrowserTestUtils.waitForMutationCondition(
    passwordLine,
    {
      attributeFilter: ["inputtype"],
    },
    () => passwordLine.getAttribute("inputtype") === "password"
  );
  return concealedPromise;
}

// Select the button within the <panel-item> element, as that is the target we want to click.
// Triggering a click event directly on the <panel-item> would fail a11y tests because
// <panel-item>  has a default role="presentation," which cannot be overridden.
const getShadowBtn = (menu, selector) =>
  menu.querySelector(selector).shadowRoot.querySelector("button");

async function testImportFromBrowser(
  passwordsSidebar,
  isFromMenuDropdown = true
) {
  const wizardReadyPromise = waitForMigrationWizard();

  info("Click on import browser button");
  if (isFromMenuDropdown) {
    const menu = passwordsSidebar.querySelector("panel-list");
    const menuButton = passwordsSidebar.querySelector(
      "#more-options-menubutton"
    );
    menuButton.click();
    await BrowserTestUtils.waitForEvent(menu, "shown");
    getShadowBtn(menu, "[action='import-from-browser']").click();
  } else {
    const importBrowserButton = passwordsSidebar.querySelector(
      ".empty-state-import-from-browser"
    );
    importBrowserButton.click();
  }

  info("Confirm migration wizard opened");
  const wizard = await wizardReadyPromise;
  ok(wizard, "migration wizard opened");

  let events = Glean.contextualManager.toolbarAction.testGetValue();
  assertCPMGleanEvent(events[0], {
    trigger: isFromMenuDropdown ? "toolbar" : "empty_state_card",
    option_name: "import_browser",
  });
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

add_task(async function test_login_line_commands() {
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  await addLocalOriginLogin();
  const passwordsSidebar = await openPasswordsSidebar();
  await checkAllLoginsRendered(passwordsSidebar);
  const list = passwordsSidebar.querySelector(".passwords-list");
  const card = list.querySelector("password-card");
  const expectedPasswordCard = {
    originLine: { value: "about:preferences#privacy" },
    usernameLine: { value: "john" },
    passwordLine: { value: "pass4" },
  };
  const selectors = Object.keys(expectedPasswordCard);

  for (const selector of selectors) {
    let loginLineInput = card[selector].shadowRoot.querySelector("input");
    if (selector === "originLine") {
      const browserLoadedPromise = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        expectedPasswordCard[selector].value
      );
      info(`click on ${selector}`);
      loginLineInput.click();
      let events = Glean.contextualManager.recordsInteraction.testGetValue();
      assertCPMGleanEvent(events[0], {
        interaction_type: "url_navigate",
      });
      await browserLoadedPromise;
      ok(true, "origin url loaded");
    } else if (selector === "usernameLine") {
      await SimpleTest.promiseClipboardChange(
        expectedPasswordCard[selector].value,
        () => {
          info(`click on ${selector}`);
          loginLineInput.click();
          let events =
            Glean.contextualManager.recordsInteraction.testGetValue();
          assertCPMGleanEvent(events[1], {
            interaction_type: "copy_username",
          });
        }
      );
    } else if (
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin() &&
      selector === "passwordLine"
    ) {
      const loginLine = card[selector].loginLine;
      loginLineInput = loginLine.shadowRoot.querySelector("input");
      await waitForReauth(() => {
        return SimpleTest.promiseClipboardChange(
          expectedPasswordCard[selector].value,
          () => {
            info(`click on ${selector}`);
            loginLineInput.click();
            let events =
              Glean.contextualManager.recordsInteraction.testGetValue();
            assertCPMGleanEvent(events[2], {
              interaction_type: "copy_password",
            });
          }
        );
      });

      const revealBtn = card[selector].revealBtn;
      const revealBtnPromise = BrowserTestUtils.waitForMutationCondition(
        loginLine,
        {
          attributeFilter: ["inputtype"],
        },
        () => loginLine.getAttribute("inputtype") === "text"
      );
      info("click on reveal button");
      revealBtn.click();

      let events = Glean.contextualManager.recordsInteraction.testGetValue();
      assertCPMGleanEvent(events[3], {
        interaction_type: "view_password",
      });

      await revealBtnPromise;
      is(
        loginLineInput.value,
        expectedPasswordCard[selector].value,
        "password revealed"
      );

      info("click on button again to conceal the password");
      revealBtn.click();
      await waitForPasswordConceal(loginLine);
      ok(true, "Password is hidden.");

      events = Glean.contextualManager.recordsInteraction.testGetValue();
      assertCPMGleanEvent(events[4], {
        interaction_type: "hide_password",
      });
    }
  }

  LoginTestUtils.clearData();
  BrowserTestUtils.addTab(gBrowser, "about:blank");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  SidebarController.hide();
});

add_task(async function test_passwords_menu_external_links() {
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  const passwordsSidebar = await openPasswordsSidebar();
  await waitForSnapshots();
  const menu = passwordsSidebar.querySelector("panel-list");
  const menuButton = passwordsSidebar.querySelector("#more-options-menubutton");

  menuButton.click();
  await BrowserTestUtils.waitForEvent(menu, "shown");

  const preferencesTabPromise = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    PREFERENCES_URL
  );

  getShadowBtn(menu, "[action='open-preferences']").click();

  let events = Glean.contextualManager.toolbarAction.testGetValue();
  assertCPMGleanEvent(events[0], {
    trigger: "toolbar",
    option_name: "preferences",
  });

  await preferencesTabPromise;
  ok(true, "passwords settings in preferences opened.");

  menuButton.click();
  await BrowserTestUtils.waitForEvent(menu, "shown");
  const helpTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, SUPPORT_URL);

  getShadowBtn(menu, "[action='open-help']").click();
  events = Glean.contextualManager.toolbarAction.testGetValue();
  assertCPMGleanEvent(events[1], {
    trigger: "toolbar",
    option_name: "help",
  });
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
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  const passwordsSidebar = await openPasswordsSidebar();
  await waitForSnapshots();
  await testImportFromBrowser(passwordsSidebar);

  BrowserTestUtils.addTab(gBrowser, "about:blank");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  SidebarController.hide();
});

add_task(async function test_passwords_menu_import_from_browser_empty_state() {
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  const passwordsSidebar = await openPasswordsSidebar();
  await waitForSnapshots();
  await testImportFromBrowser(passwordsSidebar, false);

  BrowserTestUtils.addTab(gBrowser, "about:blank");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  SidebarController.hide();
});

add_task(async function test_passwords_visibility_when_view_shown() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    ok(true, "Cannot test OSAuth.");
    return;
  }

  const login = TEST_LOGIN_1;
  await LoginTestUtils.addLogin(login);

  let megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);

  info("Test that reopening the sidebar should have password concealed.");
  let passwordCard = megalist.querySelector("password-card");
  await waitForReauth(async () => {
    return await waitForPasswordReveal(passwordCard.passwordLine);
  });

  info("Hide the sidebar");
  SidebarController.hide();
  await BrowserTestUtils.waitForCondition(() => {
    return !SidebarController.isOpen;
  }, "Sidebar did not close.");

  info("Open sidebar and check visibility of password field");
  megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);
  passwordCard = megalist.querySelector("password-card");
  await waitForPasswordConceal(passwordCard.passwordLine.loginLine);
  ok(true, "Password is hidden.");

  info(
    "Test that switching panels then switching back to Passwords should have password concealed."
  );
  await SidebarController.show("viewBookmarksSidebar");
  megalist = await openPasswordsSidebar();
  await checkAllLoginsRendered(megalist);
  passwordCard = megalist.querySelector("password-card");
  await waitForPasswordConceal(passwordCard.passwordLine.loginLine);
  ok(true, "Password is hidden.");

  SidebarController.hide();
});
