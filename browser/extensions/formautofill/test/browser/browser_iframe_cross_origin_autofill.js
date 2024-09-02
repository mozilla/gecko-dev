/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global add_heuristic_tests */

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PROFILE = {
  "cc-name": "John Doe",
  "cc-number": "4111111111111111",
  // "cc-type" should be remove from proile after fixing Bug 1834768.
  "cc-type": "visa",
  "cc-exp-month": "04",
  "cc-exp-year": new Date().getFullYear(),
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.creditCards.supported", "on"],
      ["extensions.formautofill.creditCards.enabled", true],
    ],
  });
});

add_autofill_heuristic_tests([
  {
    description: `Trigger detection in the main-frame`,
    fixtureData: `
      <p><label>Card Number: <input id="cc-number" autocomplete="cc-number"></label></p>
      <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_EXP}\"></iframe>
      <p><label>Card Type: <select id="cc-type" autocomplete="cc-type">
        <option></option>
        <option value="discover">Discover</option>
        <option value="jcb">JCB</option>
        <option value="visa">Visa</option>
        <option value="mastercard">MasterCard</option>
        <option value="gringotts">Unknown card network</option>
      </select></label></p>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-number",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
          { fieldName: "cc-exp", autofill: "" },
          { fieldName: "cc-type", autofill: "visa" },
        ],
      },
    ],
  },
  {
    description: `Trigger detection in a fist-party-origin iframe`,
    fixtureData: `
      <p><label>Card Number: <input id="cc-number" autocomplete="cc-number"></label></p>
      <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_EXP}\"></iframe>
      <iframe src=\"${SAME_ORIGIN_CC_TYPE}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
          { fieldName: "cc-exp", autofill: "" },
          { fieldName: "cc-type", autofill: "visa" },
        ],
      },
    ],
  },
  {
    description: `Trigger detection in a third-party-origin iframe`,
    fixtureData: `
      <p><label>Card Number: <input id="cc-number" autocomplete="cc-number"></label></p>
      <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_EXP}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_TYPE}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-exp",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: "" },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
          {
            fieldName: "cc-exp",
            autofill: `${TEST_PROFILE["cc-exp-month"]}/${TEST_PROFILE["cc-exp-year"]}`,
          },
          { fieldName: "cc-type", autofill: "visa" },
        ],
      },
    ],
  },
  {
    description: `Trigger detection in a third-party-origin iframe, cc-type is in another third-party-origin iframe`,
    fixtureData: `
      <p><label>Card Number: <input id="cc-number" autocomplete="cc-number"></label></p>
      <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_EXP}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_2_CC_TYPE}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-exp",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: "" },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
          {
            fieldName: "cc-exp",
            autofill: `${TEST_PROFILE["cc-exp-month"]}/${TEST_PROFILE["cc-exp-year"]}`,
          },
          { fieldName: "cc-type", autofill: "" },
        ],
      },
    ],
  },
]);
