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

add_heuristic_tests([
  {
    description: "Iframe is visible",
    fixtureData: `
    <form>
      <input id="cc-number" autocomplete="cc-number">
      <div>
          <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
      </div>
    </form>`,
    expectedResult: [
      {
        fields: [{ fieldName: "cc-number" }, { fieldName: "cc-name" }],
      },
    ],
  },
  {
    description: "Iframe is invisible",
    fixtureData: `
    <form>
      <input id="cc-number" autocomplete="cc-number">
      <div hidden="">
          <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
      </div>
    </form>`,
    expectedResult: [
      {
        fields: [{ fieldName: "cc-number" }],
      },
    ],
  },
]);
