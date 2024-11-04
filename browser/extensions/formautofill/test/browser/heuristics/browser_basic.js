/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "autocomplete_basic.html",
      expectedResult: [
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "organization" },
            { fieldName: "street-address" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" },
            { fieldName: "postal-code" },
            { fieldName: "country" },
            { fieldName: "tel" },
            { fieldName: "email" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "organization" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2", reason: "update-heuristic" },
            { fieldName: "address-line3", reason: "update-heuristic" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" },
            { fieldName: "postal-code" },
            { fieldName: "country" },
            { fieldName: "tel" },
            { fieldName: "email" },
          ],
        },
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "cc-number" },
            { fieldName: "cc-name" },
            { fieldName: "cc-exp-month" },
            { fieldName: "cc-exp-year" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "address-line1" },
            { fieldName: "address-level2" },
            { fieldName: "address-line2" },
            { fieldName: "organization" },
          ],
        },
        {
          invalid: true,
          fields: [{ fieldName: "address-line1", reason: "update-heuristic" }],
        },
      ],
    },
  ],
  "fixtures/"
);
