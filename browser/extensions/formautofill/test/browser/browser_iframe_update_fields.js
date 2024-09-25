/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global add_heuristic_tests */

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
    description: `Test name field is corrected to cc-name`,
    fixtureData: `
      <iframe src=\"${CROSS_ORIGIN_CC_NUMBER}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_EXP}\"></iframe>
      <p><label>Name: <input id="given-name"></label></p>
    `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-exp" },
          { fieldName: "cc-name", reason: "update-heuristic" },
        ],
      },
    ],
  },
]);
