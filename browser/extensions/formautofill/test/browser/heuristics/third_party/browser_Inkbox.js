/* global add_heuristic_tests */

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

add_autofill_heuristic_tests(
  [
    {
      fixturePath: "checkouts.html",
      autofillTrigger: "input[id=number]", // The first frame
      profile: TEST_PROFILE,
      expectedResult: [
        {
          default: {
            addressType: "shipping",
          },
          fields: [
            { fieldName: "email" },
            { fieldName: "country" },
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "organization" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "postal-code" },
            { fieldName: "address-level2" },
            { fieldName: "tel" },
          ],
        },
        {
          fields: [
            { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
            { fieldName: "cc-exp", autofill: `${TEST_PROFILE["cc-exp-month"]}/${TEST_PROFILE["cc-exp-year"].toString().substr(-2)}` },
            { fieldName: "cc-csc", },
            { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/Inkbox/"
);
