/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.megalist.enabled", true],
      ["signon.rememberSignons", true],
    ],
  });

  registerCleanupFunction(function () {
    LoginTestUtils.clearData();
  });
});

add_task(async function test_passwords_sidebar() {
  await addMockPasswords();

  info("Open Passwords sidebar");
  await SidebarController.show("viewMegalistSidebar");
  const sidebar = document.getElementById("sidebar");
  const megalist =
    sidebar.contentDocument.querySelector("megalist-alpha").shadowRoot;

  info("Check that records are rendered");
  await BrowserTestUtils.waitForMutationCondition(
    megalist,
    { childList: true, subtree: true },
    () => {
      const passwordsList = megalist.querySelector(".passwords-list");
      return passwordsList?.querySelectorAll("password-card").length === 3;
    }
  );
  ok(true, "3 password cards are rendered.");

  info("Check correct initial login info is rendered.");
  checkPasswordCardFields(megalist);

  LoginTestUtils.clearData();
});

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
