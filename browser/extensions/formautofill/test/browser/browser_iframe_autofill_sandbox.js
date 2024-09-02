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
      ["ui.popup.disable_autohide", true],
      ["extensions.formautofill.creditCards.supported", "on"],
      ["extensions.formautofill.creditCards.enabled", true],
    ],
  });
});

add_autofill_heuristic_tests([
  {
    description:
      "Trigger autofill in the main-frame, do not autofill into sandboxed iframe",
    fixtureData: `
      <p><label>Card Number: <input id="cc-number" autocomplete="cc-number"></label></p>
      <iframe sandbox src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
      <iframe sandbox src=\"${CROSS_ORIGIN_CC_EXP}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-number",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          { fieldName: "cc-name", autofill: "" },
          { fieldName: "cc-exp", autofill: "" },
        ],
      },
    ],
  },
  {
    description:
      "Trigger autofill in a first-party-origin iframe, do not autofill into sandboxed iframe",
    fixtureData: `
      <iframe sandbox src=\"${SAME_ORIGIN_CC_NUMBER}\"></iframe>
      <p><label>Card Name: <input id="cc-name" autocomplete="cc-name"></label></p>
      <iframe sandbox src=\"${SAME_ORIGIN_CC_EXP}\"></iframe>
      <iframe sandbox src=\"${CROSS_ORIGIN_CC_TYPE}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-number",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          { fieldName: "cc-name", autofill: "" },
          { fieldName: "cc-exp", autofill: "" },
          { fieldName: "cc-type", autofill: "" },
        ],
      },
    ],
  },
  {
    description:
      "Trigger autofill in a sandboxed first-party-origin iframe, do not autofill into iframe",
    fixtureData: `
      <iframe sandbox src=\"${SAME_ORIGIN_CC_NUMBER}\"></iframe>
      <p><label>Card Name: <input id="cc-name" autocomplete="cc-name"></label></p>
      <iframe src=\"${SAME_ORIGIN_CC_EXP}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_TYPE}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-number",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          { fieldName: "cc-name", autofill: "" },
          { fieldName: "cc-exp", autofill: "" },
          { fieldName: "cc-type", autofill: "" },
        ],
      },
    ],
  },
  {
    description:
      "Trigger autofill in a third-party-origin iframe, do not autofill into sandboxed other iframes",
    fixtureData: `
      <iframe src=\"${CROSS_ORIGIN_CC_NUMBER}\"></iframe>
      <p><label>Card Name: <input id="cc-name" autocomplete="cc-name"></label></p>
      <iframe sandbox src=\"${SAME_ORIGIN_CC_EXP}\"></iframe>
      <iframe sandbox src=\"${CROSS_ORIGIN_CC_TYPE}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-number",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          { fieldName: "cc-name", autofill: "" },
          { fieldName: "cc-exp", autofill: "" },
          { fieldName: "cc-type", autofill: "" },
        ],
      },
    ],
  },
  {
    description:
      "Trigger autofill in a sandboxed third-party-origin iframe, do not autofill into other iframes",
    fixtureData: `
      <iframe sandbox src=\"${CROSS_ORIGIN_CC_NUMBER}\"></iframe>
      <p><label>Card Name: <input id="cc-name" autocomplete="cc-name"></label></p>
      <iframe src=\"${SAME_ORIGIN_CC_EXP}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_TYPE}\"></iframe>
    `,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-number",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          { fieldName: "cc-name", autofill: "" },
          { fieldName: "cc-exp", autofill: "" },
          { fieldName: "cc-type", autofill: "" },
        ],
      },
    ],
  },
  {
    description:
      "Relaxed restriction applied - Trigger autofill in a sandboxed third-party-origin iframe, do not autofill into other iframes",
    fixtureData: `
      <iframe sandbox src=\"${CROSS_ORIGIN_CC_NUMBER}\"></iframe>
      <p><label>Card Name: <input id="cc-name" autocomplete="cc-name"></label></p>
      <iframe src=\"${SAME_ORIGIN_CC_EXP}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_TYPE}\"></iframe>
    `,
    prefs: [
      ["extensions.formautofill.heuristics.autofillSameOriginWithTop", true],
    ],
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-number",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
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
