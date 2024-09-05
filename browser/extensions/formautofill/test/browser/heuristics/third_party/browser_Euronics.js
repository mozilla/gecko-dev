/* global runHeuristicsTest */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "Payment.html",
      expectedResult: [
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "cc-number" },
            { fieldName: "cc-csc", reason: "regex-heuristic" },
            { fieldName: "cc-exp-month" },
            { fieldName: "cc-exp-year" },
            { fieldName: "cc-type", reason: "regex-heuristic" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "email" },
            { fieldName: "tel" },
            { fieldName: "country" },
            { fieldName: "organization" },
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/Euronics/"
);
