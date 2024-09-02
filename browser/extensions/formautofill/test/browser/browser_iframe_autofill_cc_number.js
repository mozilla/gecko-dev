/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global add_autofill_heuristic_tests */

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
  /**
   * Test credit card number in the main-frame
   */
  {
    description:
      "Fill cc-number in a main-frame when the autofill is triggered in a first-party-origin iframe",
    fixtureData: `
      <p><label>Card Number: <input id="cc-number" autocomplete="cc-number"></label></p>
      <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
        ],
      },
    ],
  },
  {
    description:
      "Do not fill cc-number in a main-frame when the autofill is triggered in a third-party-origin iframe",
    fixtureData: `
      <p><label>Card Number: <input id="cc-number" autocomplete="cc-number"></label></p>
      <iframe src=\"${CROSS_ORIGIN_CC_NAME}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: "" },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
        ],
      },
    ],
  },
  /**
   * Test credit card number in a first-party-origin iframe
   */
  {
    description:
      "Fill cc-number in a first-party-origin iframe when the autofill is triggered in another first-party-origin iframe",
    fixtureData: `
      <iframe src=\"${SAME_ORIGIN_CC_NUMBER}\"></iframe>
      <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
        ],
      },
    ],
  },
  {
    description:
      "Do not fill cc-number in a first-party-origin iframe when autofill is triggered in a third-party iframe",
    fixtureData: `
      <iframe src=\"${SAME_ORIGIN_CC_NUMBER}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_NAME}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: "" },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
        ],
      },
    ],
  },
  {
    description:
      "Fill cc-number in a first-party-origin iframe when the autofill is triggered in the main-frame",
    fixtureData: `
      <iframe src=\"${SAME_ORIGIN_CC_NUMBER}\"></iframe>
      <p><label>Card Name: <input id="cc-name" autocomplete="cc-name"></label></p>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
        ],
      },
    ],
  },
  /**
   * Test credit card number is in a third-party iframe
   */
  {
    description:
      "Do not fill cc-number in a third-party-origin iframe when the autofill is triggered in a first-party-origin iframe",
    fixtureData: `
      <iframe src=\"${CROSS_ORIGIN_CC_NUMBER}\"></iframe>
      <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: "" },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
        ],
      },
    ],
  },
  {
    description:
      "Do not fill cc-number in a third-party-origin iframe when the autofill is triggered in cross-origin third-party-origin iframe",
    fixtureData: `
      <iframe src=\"${CROSS_ORIGIN_CC_NUMBER}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_2_CC_NAME}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: "" },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
        ],
      },
    ],
  },
  {
    description:
      "Do not fill cc-number in a third-party-origin iframe when the autofill is triggered in the main-iframe",
    fixtureData: `
      <iframe src=\"${CROSS_ORIGIN_CC_NUMBER}\"></iframe>
      <p><label>Card Name: <input id="cc-name" autocomplete="cc-name"></label></p>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: "" },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
        ],
      },
    ],
  },
  {
    description:
      "Fill cc-number in a third-party-origin iframe when the autofill is triggered in a same-origin third-party-origin iframe",
    fixtureData: `
      <iframe src=\"${CROSS_ORIGIN_CC_NUMBER}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_NAME}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
        ],
      },
    ],
  },
  /**
   * Test cases when relaxed restriction is applied
   */
  {
    description:
      "Do not fill cc-number in a main-frame when the autofill is triggered in a third-party-origin iframe even when the relaxed restriction is applied",
    fixtureData: `
      <p><label>Card Number: <input id="cc-number" autocomplete="cc-number"></label></p>
      <iframe src=\"${CROSS_ORIGIN_CC_NAME}\"></iframe>
    `,
    prefs: [
      ["extensions.formautofill.heuristics.autofillSameOriginWithTop", true],
    ],
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: "" },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
        ],
      },
    ],
  },
  {
    description:
      "Do not fill cc-number in a first-party-origin iframe when autofill is triggered in a third-party iframe even when the relaxed restriction is applied",
    fixtureData: `
      <iframe src=\"${SAME_ORIGIN_CC_NUMBER}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_NAME}\"></iframe>
    `,
    prefs: [
      ["extensions.formautofill.heuristics.autofillSameOriginWithTop", true],
    ],
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: "" },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
        ],
      },
    ],
  },
  {
    description:
      "Do not fill cc-number in a third-party-origin iframe when the autofill is triggered in cross-origin third-party-origin iframe even when the relaxed restriction is applied",
    fixtureData: `
      <iframe src=\"${CROSS_ORIGIN_CC_NUMBER}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_2_CC_NAME}\"></iframe>
    `,
    prefs: [
      ["extensions.formautofill.heuristics.autofillSameOriginWithTop", true],
    ],
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: "" },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
        ],
      },
    ],
  },
  {
    description:
      "Do not fill cc-number in a third-party-origin iframe when the autofill is triggered in the main-iframe even when the relaxed restriction is applied",
    fixtureData: `
      <iframe src=\"${CROSS_ORIGIN_CC_NUMBER}\"></iframe>
      <p><label>Card Name: <input id="cc-name" autocomplete="cc-name"></label></p>
    `,
    prefs: [
      ["extensions.formautofill.heuristics.autofillSameOriginWithTop", true],
    ],
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-name",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: "" },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
        ],
      },
    ],
  },
]);
