/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TESTCASES = [
  {
    description: "Clear populated address form with text inputs",
    document: `<form>
                <input id="given-name">
                <input id="family-name">
                <input id="street-address">
                <input id="address-level2">
               </form>`,
    focusedInputId: "given-name",
    profileData: {
      "given-name": "John",
      "family-name": "Doe",
      "street-address": "1000 Main Street",
      "address-level2": "Nowhere",
    },
    expectedResult: {
      "given-name": "",
      "family-name": "",
      "street-address": "",
      "address-level2": "",
    },
  },
  {
    description: "Clear populated address form with select and text inputs",
    document: `<form>
                <input id="given-name">
                <input id="family-name">
                <input id="street-address">
                <select id="address-level1">
                  <option value="AL">Alabama</option>
                  <option value="AK">Alaska</option>
                  <option value="OH">Ohio</option>
                </select>
               </form>`,
    focusedInputId: "given-name",
    profileData: {
      "given-name": "John",
      "family-name": "Doe",
      "street-address": "1000 Main Street",
      "address-level1": "OH",
    },
    expectedResult: {
      "given-name": "",
      "family-name": "",
      "street-address": "",
      "address-level1": "AL",
    },
  },
  {
    description:
      "Clear populated address form with select element with selected attribute and text inputs",
    document: `<form>
                <input id="given-name">
                <input id="family-name">
                <input id="street-address">
                <select id="address-level1">
                  <option value="AL">Alabama</option>
                  <option selected value="AK">Alaska</option>
                  <option value="OH">Ohio</option>
                </select>
               </form>`,
    focusedInputId: "given-name",
    profileData: {
      "given-name": "John",
      "family-name": "Doe",
      "street-address": "1000 Main Street",
      "address-level1": "OH",
    },
    expectedResult: {
      "given-name": "",
      "family-name": "",
      "street-address": "",
      "address-level1": "AK",
    },
  },
];

add_task(async function test_clear_populated_fields() {
  for (const TEST of TESTCASES) {
    info("TEST case: " + TEST.description);

    await setStorage(TEST.profileData);

    const TEST_URL =
      "https://example.org/document-builder.sjs?html=" + TEST.document;
    await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
      await openPopupOn(browser, `#${TEST.focusedInputId}`);

      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await waitForAutofill(
        browser,
        `#${TEST.focusedInputId}`,
        TEST.profileData[TEST.focusedInputId]
      );

      await waitForAutoCompletePopupOpen(browser, async () => {
        await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      });
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);

      await SpecialPowers.spawn(browser, [TEST], async obj => {
        const focusedElement = content.document.getElementById(
          obj.focusedInputId
        );
        // Ensure the child process has enough time to clear the fields
        await ContentTaskUtils.waitForCondition(
          () => focusedElement.value == obj.expectedResult[obj.focusedInputId]
        );

        for (const [id, value] of Object.entries(obj.expectedResult)) {
          const element = content.document.getElementById(id);
          Assert.equal(
            element.value,
            value,
            `${id} field was restored to the correct value`
          );
        }
      });
    });

    await removeAllRecords();
  }
});
