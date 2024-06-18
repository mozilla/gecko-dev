"use strict";

// This test verifies that the address fillin popup appears correctly when
// the required fields have been given values.

const { Region } = ChromeUtils.importESModule(
  "resource://gre/modules/Region.sys.mjs"
);

async function expectSavedAddresses(expectedCount) {
  const addresses = await getAddresses();
  is(
    addresses.length,
    expectedCount,
    `${addresses.length} address in the storage`
  );
  return addresses;
}

// Test when all required fields are present.
add_task(
  async function test_doorhanger_shown_when_contain_all_required_fields() {
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        let onPopupShown = waitForPopupShown();

        await focusUpdateSubmitForm(browser, {
          focusSelector: "#street-address",
          newValues: {
            "#street-address": "32 Vassar Street\nMIT Room 32-G524",
            "#postal-code": "02139",
            "#address-level2": "Cambridge",
            "#address-level1": "MA",
          },
        });

        await onPopupShown;
        await clickDoorhangerButton(MAIN_BUTTON, 0);
      }
    );

    await expectSavedAddresses(1);
    await removeAllRecords();
  }
);

// Test when a required field is invalid.
add_task(
  async function test_doorhanger_not_shown_when_contain_required_invalid_fields() {
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        await focusUpdateSubmitForm(browser, {
          focusSelector: "#street-address",
          newValues: {
            "#street-address": "32 Vassar Street\nMIT Room 32-G524",
            "#postal-code": "000", // postal-code is invalid
            "#address-level2": "Cambridge",
            "#address-level1": "MA",
          },
        });

        is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
      }
    );
  }
);

// Test when all not required fields are present.
add_task(
  async function test_doorhanger_not_shown_when_not_contain_all_required_fields() {
    await SpecialPowers.pushPrefEnv({
      clear: [["extensions.formautofill.addresses.capture.requiredFields"]],
    });

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        await focusUpdateSubmitForm(browser, {
          focusSelector: "#street-address",
          newValues: {
            "#given-name": "John",
            "#family-name": "Doe",
            "#postal-code": "02139",
            "#address-level2": "Cambridge",
            "#address-level1": "MA",
          },
        });

        is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
      }
    );
  }
);

// Test using a region that only requires street address and address-level1
// and both are present.
add_task(
  async function test_doorhanger_shown_when_all_required_fields_other_region() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["extensions.formautofill.addresses.supportedCountries", "US,CA,ID"],
      ],
    });

    const initialHomeRegion = Region._home;
    const initialCurrentRegion = Region._current;

    const region = "ID";
    Region._setCurrentRegion(region);
    Region._setHomeRegion(region);

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        let onPopupShown = waitForPopupShown();

        await focusUpdateSubmitForm(browser, {
          focusSelector: "#street-address",
          newValues: {
            "#given-name": "John",
            "#family-name": "Doe",
            "#street-address": "Randomly Somewhere",
            "#address-level1": "Sumatera Utara",
          },
        });

        await onPopupShown;
        await clickDoorhangerButton(MAIN_BUTTON, 0);
      }
    );

    await expectSavedAddresses(1);

    Region._setCurrentRegion(initialHomeRegion);
    Region._setHomeRegion(initialCurrentRegion);

    await SpecialPowers.popPrefEnv();
    await removeAllRecords();
  }
);

// Test using a region that only requires street address and address-level1
// and only one of those is present.
add_task(
  async function test_doorhanger_not_shown_when_notall_required_fields_other_region() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["extensions.formautofill.addresses.supportedCountries", "US,CA,ID"],
      ],
    });

    const initialHomeRegion = Region._home;
    const initialCurrentRegion = Region._current;

    const region = "ID";
    Region._setCurrentRegion(region);
    Region._setHomeRegion(region);

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        await focusUpdateSubmitForm(browser, {
          focusSelector: "#street-address",
          newValues: {
            "#given-name": "John",
            "#family-name": "Doe",
            "#address-level1": "Sumatera Utara",
          },
        });

        is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
      }
    );

    Region._setCurrentRegion(initialHomeRegion);
    Region._setHomeRegion(initialCurrentRegion);

    await SpecialPowers.popPrefEnv();
    await removeAllRecords();
  }
);

// Test when the preference is used.
add_task(
  async function test_doorhanger_shown_when_contain_all_required_fields_withpref() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "extensions.formautofill.addresses.capture.requiredFields",
          "street-address,postal-code,address-level1",
        ],
      ],
    });

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        let onPopupShown = waitForPopupShown();

        await focusUpdateSubmitForm(browser, {
          focusSelector: "#street-address",
          newValues: {
            "#street-address": "32 Vassar Street\nMIT Room 32-G524",
            "#postal-code": "02139",
            "#address-level1": "MA",
          },
        });

        await onPopupShown;
        await clickDoorhangerButton(MAIN_BUTTON, 0);
      }
    );

    await expectSavedAddresses(1);
    await SpecialPowers.popPrefEnv();
    await removeAllRecords();
  }
);
