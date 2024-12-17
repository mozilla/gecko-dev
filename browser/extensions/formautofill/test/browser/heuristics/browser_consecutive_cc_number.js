/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

add_heuristic_tests([
  {
    description:
      "Form containing multiple cc-number fields without autocomplete attributes.",
    fixtureData: `<form>
                  <input id="cc-number1" maxlength="4">
                  <input id="cc-number2" maxlength="4">
                  <input id="cc-number3" maxlength="4">
                  <input id="cc-number4" maxlength="4">
                  <input id="cc-name">
                  <input id="cc-exp-month">
                  <input id="cc-exp-year">
                 </form>`,
    expectedResult: [
      {
        default: {
          reason: "fathom",
        },
        fields: [
          { fieldName: "cc-number", part: 1 },
          { fieldName: "cc-number", part: 2 },
          { fieldName: "cc-number", part: 3 },
          { fieldName: "cc-number", part: 4 },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp-month", reason: "regex-heuristic" },
          { fieldName: "cc-exp-year", reason: "regex-heuristic" },
        ],
      },
    ],
  },
  {
    description:
      "Invalid form containing three consecutive cc-number fields without autocomplete attributes.",
    fixtureData: `<form>
                  <input id="cc-number1" maxlength="4">
                  <input id="cc-number2" maxlength="4">
                  <input id="cc-number3" maxlength="4">
                 </form>`,
    expectedResult: [
      {
        invalid: true,
        fields: [
          { fieldName: "cc-number", reason: "fathom" },
          { fieldName: "cc-number", reason: "fathom" },
          { fieldName: "cc-number", reason: "fathom" },
        ],
      },
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "1.0",
      ],
    ],
  },
  {
    description:
      "Invalid form containing five consecutive cc-number fields without autocomplete attributes.",
    fixtureData: `<form>
                  <input id="cc-number1" maxlength="4">
                  <input id="cc-number2" maxlength="4">
                  <input id="cc-number3" maxlength="4">
                  <input id="cc-number4" maxlength="4">
                  <input id="cc-number5" maxlength="4">
                 </form>`,
    expectedResult: [
      {
        default: {
          reason: "fathom",
        },
        invalid: true,
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-number", part: 1 },
          { fieldName: "cc-number", part: 2 },
          { fieldName: "cc-number", part: 3 },
          { fieldName: "cc-number", part: 4 },
        ],
      },
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "1.0",
      ],
    ],
  },
  {
    description:
      "Valid form containing three consecutive cc-number fields without autocomplete attributes.",
    fixtureData: `<form>
                  <input id="cc-number1" maxlength="4">
                  <input id="cc-number2" maxlength="4">
                  <input id="cc-number3" maxlength="4">
                  <input id="cc-name">
                  <input id="cc-exp-month">
                  <input id="cc-exp-year">
                 </form>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", reason: "fathom" },
          { fieldName: "cc-number", reason: "fathom" },
          { fieldName: "cc-number", reason: "fathom" },
          { fieldName: "cc-name", reason: "fathom" },
          { fieldName: "cc-exp-month", reason: "regex-heuristic" },
          { fieldName: "cc-exp-year", reason: "regex-heuristic" },
        ],
      },
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "1.0",
      ],
    ],
  },
  {
    description:
      "Valid form containing five consecutive cc-number fields without autocomplete attributes.",
    fixtureData: `<form>
                  <input id="cc-number1" maxlength="4">
                  <input id="cc-number2" maxlength="4">
                  <input id="cc-number3" maxlength="4">
                  <input id="cc-number4" maxlength="4">
                  <input id="cc-number5" maxlength="4">
                  <input id="cc-name">
                  <input id="cc-exp-month">
                  <input id="cc-exp-year">
                 </form>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", reason: "fathom" },
          { fieldName: "cc-number", part: 1, reason: "fathom" },
          { fieldName: "cc-number", part: 2, reason: "fathom" },
          { fieldName: "cc-number", part: 3, reason: "fathom" },
          { fieldName: "cc-number", part: 4, reason: "fathom" },
          { fieldName: "cc-name", reason: "fathom" },
          { fieldName: "cc-exp-month", reason: "regex-heuristic" },
          { fieldName: "cc-exp-year", reason: "regex-heuristic" },
        ],
      },
    ],
  },
]);
