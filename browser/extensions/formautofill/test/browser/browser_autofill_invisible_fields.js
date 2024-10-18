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
  {
    description:
      "Do not autofill invisible <input> but autofill invisibile <select>",
    fixtureData: `
    <form>
      <input id="cc-number" autocomplete="cc-number">
      <input id="cc-name-invisible" autocomplete="cc-name" style="display: none;">
      <input id="cc-exp" autocomplete="cc-exp">
      <select id="cc-type" autocomplete="cc-type" style="display: none;">
        <option value="0" selected="">Card Type</option>
        <option value="visa">Visa</option>
        <option value="master">MasterCard</option>
     </select>
    </form>`,
    profile: TEST_PROFILE,
    autofillTrigger: "#cc-number",
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          {
            fieldName: "cc-exp",
            autofill: `${TEST_PROFILE["cc-exp-month"]}/${TEST_PROFILE["cc-exp-year"]}`,
          },
          { fieldName: "cc-type", autofill: TEST_PROFILE["cc-type"] },
        ],
      },
    ],
  },
]);
