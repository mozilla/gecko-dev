"use strict";

add_task(async function test_doorhanger_shown_with_lowercase_postalcode() {
  const initialHomeRegion = Region._home;
  const initialCurrentRegion = Region._current;

  const region = "CA";
  Region._setCurrentRegion(region);
  Region._setHomeRegion(region);

  const ADDRESS_FIELD_VALUES = {
    "given-name": "John",
    "family-name": "Doe",
    "street-address": "123 Sesame Street",
    "address-level1": "Ontario",
    "address-level2": "Ottawa",
    "postal-code": "K1A 0A6",
  };

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#street-address",
        newValues: {
          "#given-name": ADDRESS_FIELD_VALUES["given-name"],
          "#family-name": ADDRESS_FIELD_VALUES["family-name"],
          "#street-address": ADDRESS_FIELD_VALUES["street-address"],
          "#address-level1": ADDRESS_FIELD_VALUES["address-level1"],
          "#address-level2": ADDRESS_FIELD_VALUES["address-level2"],
          "#postal-code": ADDRESS_FIELD_VALUES["postal-code"],
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON, 0);
    }
  );

  await expectSavedAddresses([ADDRESS_FIELD_VALUES]);

  const ADDRESS_FIELD_VALUES_LOWERCASE = {
    "given-name": "Jane",
    "family-name": "Doe",
    "street-address": "123 Sesame Street",
    "address-level1": "Ontario",
    "address-level2": "Ottawa",
    "postal-code": "k1a 0a6",
  };

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#street-address",
        newValues: {
          "#given-name": ADDRESS_FIELD_VALUES_LOWERCASE["given-name"],
          "#family-name": ADDRESS_FIELD_VALUES_LOWERCASE["family-name"],
          "#street-address": ADDRESS_FIELD_VALUES_LOWERCASE["street-address"],
          "#address-level1": ADDRESS_FIELD_VALUES_LOWERCASE["address-level1"],
          "#address-level2": ADDRESS_FIELD_VALUES_LOWERCASE["address-level2"],
          "#postal-code": ADDRESS_FIELD_VALUES_LOWERCASE["postal-code"],
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON, 0);
    }
  );

  await expectSavedAddresses([
    ADDRESS_FIELD_VALUES,
    ADDRESS_FIELD_VALUES_LOWERCASE,
  ]);

  Region._setCurrentRegion(initialHomeRegion);
  Region._setHomeRegion(initialCurrentRegion);

  await SpecialPowers.popPrefEnv();
  await removeAllRecords();
});
