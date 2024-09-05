/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "Checkout_Payment.html",
      expectedResult: [
        {
          default: {
            reason: "fathom",
          },
          fields: [
            { fieldName: "cc-type", reason: "regex-heuristic" },
            { fieldName: "cc-number", part: 1 },
            { fieldName: "cc-number", part: 2 },
            { fieldName: "cc-number", part: 3 },
            { fieldName: "cc-number", part: 4 },
            { fieldName: "cc-exp-month", reason: "regex-heuristic" },
            { fieldName: "cc-exp-year", reason: "regex-heuristic" },
            { fieldName: "cc-csc", reason: "regex-heuristic" },
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/Lufthansa/"
);
