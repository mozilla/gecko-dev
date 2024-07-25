/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Need to sync with `autofillState` attribute in HTMLInputElement
const PREVIEW = "preview";
const NORMAL = "";

const TESTCASES = [
  {
    description: "Preview best case address form",
    document: `<form>
    <input id="given-name" autocomplete="given-name">
    <input id="family-name" autocomplete="family-name">
    <input id="street-address" autocomplete="street-address">
    <input id="address-level2" autocomplete="address-level2">
   </form>`,
    focusedInputId: "given-name",
    profileData: {
      "given-name": "John",
      "family-name": "Doe",
      "street-address": "100 Main Street",
      "address-level2": "Hamilton",
    },
    expectedResultState: {
      "given-name": [PREVIEW],
      "family-name": [PREVIEW],
      "street-address": [PREVIEW],
      "address-level2": [PREVIEW],
    },
  },
  {
    description: "Preview form with a readonly input and non-readonly inputs",
    document: `<form>
    <input id="given-name" autocomplete="given-name">
    <input id="family-name" autocomplete="family-name">
    <input id="street-address" autocomplete="street-address">
    <input id="address-level2" autocomplete="address-level2" readonly value="TEST CITY">
   </form>`,
    focusedInputId: "given-name",
    profileData: {
      "given-name": "John",
      "family-name": "Doe",
      "street-address": "100 Main Street",
      "address-level2": "Hamilton",
    },
    expectedResultState: {
      "given-name": [PREVIEW],
      "family-name": [PREVIEW],
      "street-address": [PREVIEW],
      "address-level2": [NORMAL],
    },
  },
  {
    description: "Preview form with a disabled input and non-disabled inputs",
    document: `<form>
    <input id="given-name" autocomplete="given-name">
    <input id="family-name" autocomplete="family-name">
    <input id="street-address" autocomplete="street-address">
    <input id="country" autocomplete="country" disabled value="US">
   </form>`,
    focusedInputId: "given-name",
    profileData: {
      "given-name": "John",
      "family-name": "Doe",
      "street-address": "100 Main Street",
      country: "CA",
    },
    expectedResultState: {
      "given-name": [PREVIEW],
      "family-name": [PREVIEW],
      "street-address": [PREVIEW],
      country: [NORMAL],
    },
  },
  {
    description:
      "Preview form with autocomplete select elements and matching option values",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input id="organization" autocomplete="shipping organization">
               <select id="country" autocomplete="shipping country">
                 <option value=""></option>
                 <option value="US">United States</option>
               </select>
               <select id="address-level1" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="CA">California</option>
                 <option value="WA">Washington</option>
               </select>
               </form>`,
    focusedInputId: "organization",
    profileData: {
      country: "US",
      organization: "Mozilla",
      "address-level1": "CA",
    },
    expectedResultState: {
      "given-name": [NORMAL],
      organization: [PREVIEW],
      country: [PREVIEW, "United States"],
      "address-level1": [PREVIEW, "California"],
    },
  },
  {
    description: "Preview best case credit card form",
    document: `<form>
              <input id="cc-number" autocomplete="cc-number">
              <input id="cc-name" autocomplete="cc-name">
              <input id="cc-exp-month" autocomplete="cc-exp-month">
              <input id="cc-exp-year" autocomplete="cc-exp-year">
              <input id="cc-csc" autocomplete="cc-csc">
              </form>
              `,
    focusedInputId: "cc-number",
    profileData: {
      "cc-number": "4111111111111111",
      "cc-name": "test name",
      "cc-exp-month": 6,
      "cc-exp-year": 25,
    },
    expectedResultState: {
      "cc-number": [PREVIEW, "••••••••••••1111"],
      "cc-name": [PREVIEW],
      "cc-exp-month": [PREVIEW, 6],
      "cc-exp-year": [PREVIEW, 2025],
      "cc-csc": [NORMAL],
    },
  },
];

add_task(async function test_preview_form_fields() {
  for (const TEST of TESTCASES) {
    info("TEST case: " + TEST.description);
    await setStorage(TEST.profileData);

    const TEST_URL =
      "https://example.org/document-builder.sjs?html=" + TEST.document;
    await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
      const previewCompeletePromise = TestUtils.topicObserved(
        "formautofill-preview-complete"
      );
      await openPopupOn(browser, `#${TEST.focusedInputId}`);
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await previewCompeletePromise;

      // Check preview state & value
      await SpecialPowers.spawn(browser, [TEST], async obj => {
        for (const [id, expected] of Object.entries(obj.expectedResultState)) {
          info(`Checking element ${id} state`);

          const element = content.document.getElementById(id);
          let [expectedState, expectedValue] = expected;
          Assert.equal(
            element.autofillState,
            expectedState,
            "Check if preview state is set correctly"
          );

          if (expectedState == "preview") {
            expectedValue ||= obj.profileData[id];
          } else {
            expectedValue = "";
          }

          Assert.equal(
            element.previewValue,
            expectedValue,
            "Check if preview value is set correctly"
          );
        }
      });
    });

    await removeAllRecords();
  }
});
