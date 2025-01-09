/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function performTest(region, expectedCount) {
  let regionInfo = { home: Region._home, current: Region._current };
  Region._setCurrentRegion(region);
  Region._setHomeRegion(region);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: FORM_URL },
    async browser => {
      const focusInput = "#street-address";

      if (expectedCount) {
        await openPopupOn(browser, focusInput);

        let items = getDisplayedPopupItems(browser);
        // There should be up to five items: three addresses, the status row and the manage item.
        is(items.length, expectedCount, "three items in autocomplete list");
        is(
          items[0].getAttribute("ac-label"),
          TEST_ADDRESS_1["street-address"].replace("\n", " ")
        );
        is(
          items[1].getAttribute("ac-label"),
          TEST_ADDRESS_CA_1["street-address"].replace("\n", " ")
        );
        if (expectedCount >= 5) {
          is(
            items[2].getAttribute("ac-label"),
            TEST_ADDRESS_DE_1["street-address"].replace("\n", " ")
          );
        }
      } else {
        await focusAndWaitForFieldsIdentified(browser, focusInput);
        await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
        await ensureNoAutocompletePopup(browser);
      }
    }
  );

  Region._setCurrentRegion(regionInfo.home);
  Region._setHomeRegion(regionInfo.current);
}

add_setup(async function () {
  await setStorage(TEST_ADDRESS_1, TEST_ADDRESS_CA_1, TEST_ADDRESS_DE_1);
});

add_task(async function test_region_detect() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.experiments.enabled", false],
      ["extensions.formautofill.addresses.supported", "detect"],
      ["extensions.formautofill.addresses.supportedCountries", "US,CA"],
    ],
  });

  // When extensions.formautofill.addresses.supported is "detect", and the
  // current region is supported, the autocomplete items from a unsupported
  // region should not appear in the dropdown. If the current region is not
  // supported, then address autocomplete is not available.
  await performTest("CA", 4);
  await performTest("DE", 0);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_region_on() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.experiments.enabled", false],
      ["extensions.formautofill.addresses.supported", "on"],
      ["extensions.formautofill.addresses.supportedCountries", "US,CA"],
    ],
  });

  // When extensions.formautofill.addresses.supported is "on", all of the
  // autocomplete items should appear in the list.
  await performTest("CA", 5);
  await performTest("DE", 5);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_region_experiments_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.experiments.enabled", true],
      ["extensions.formautofill.addresses.supported", "detect"],
      ["extensions.formautofill.addresses.supportedCountries", "US,CA"],
    ],
  });

  // When experiments are enabled, autocomplete is enabled in all regions.
  await performTest("CA", 5);
  await performTest("DE", 5);

  await SpecialPowers.popPrefEnv();
});
