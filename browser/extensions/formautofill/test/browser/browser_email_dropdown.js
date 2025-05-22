"use strict";

const PAGE_URL =
  "https://example.org/browser/browser/extensions/formautofill/test/fixtures/autocomplete_multiple_emails_checkout.html";

// This testcase is to ensure that if a field gets recoginised by both
// login manager and formautofill providers, that if an address is saved,
// that the formautofill popup gets priority over the login manager.

add_task(async function test_email_field_is_address_dropdown() {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.rememberSignons", true]],
  });
  // If an address is saved, show the formautofill dropdown.
  await setStorage(TEST_ADDRESS_1);
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_URL },
    async function (browser) {
      const focusInput = "#email";
      // We need to initialize and identify fields on a field that doesn't trigger
      // a login autocomplete on focus, otherwise the popup could appear too early.
      await focusAndWaitForFieldsIdentified(browser, "#given-name");
      await openPopupOn(browser, focusInput);
      const item = getDisplayedPopupItems(browser)[2];

      is(
        item.getAttribute("ac-value"),
        "Manage addresses",
        "Address popup should show a valid email suggestion"
      );

      await closePopup(browser);
    }
  );
});

add_task(
  async function test_email_field_shows_login_dropdown_when_no_saved_address() {
    // However, if no addresses are saved, show the login manager.
    await removeAllRecords();
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: PAGE_URL },
      async function (browser) {
        const focusInput = "#email";
        await focusAndWaitForFieldsIdentified(browser, "#given-name");
        await openPopupOn(browser, focusInput);
        const item = getDisplayedPopupItems(browser)[0];

        is(
          item.getAttribute("ac-value"),
          "Manage Passwords",
          "Login Manager should be shown"
        );

        await closePopup(browser);
      }
    );
  }
);
