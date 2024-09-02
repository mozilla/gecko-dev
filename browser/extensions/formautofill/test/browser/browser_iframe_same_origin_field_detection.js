/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global add_heuristic_tests */

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
    description: `All fields are in the same same-origin iframe`,
    fixtureData: `<iframe src=${SAME_ORIGIN_ALL_FIELDS}></iframe>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: `cc-name in the main frame, others in its own iframes`,
    fixtureData: `
      <iframe src=\"${SAME_ORIGIN_CC_NUMBER}\"></iframe>
      <p><label>Card Name: <input id="cc-name" autocomplete="cc-name"></label></p>
      <iframe src=\"${SAME_ORIGIN_CC_EXP}\"></iframe>
    `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: `cc-name & cc-exp in the main frame, cc-number in an iframe`,
    fixtureData: `
      <iframe src=\"${SAME_ORIGIN_CC_NUMBER}\"></iframe>
      <p><label>Card Name: <input id="cc-name" autocomplete="cc-name"></label></p>
      <p><label>Card Expiration Date: <input id="cc-exp" autocomplete="cc-exp"></label></p>
    `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: `Every field is in its own same-origin iframe`,
    fixtureData: `
      <iframe src=\"${SAME_ORIGIN_CC_NUMBER}\"></iframe>
      <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
      <iframe src=\"${SAME_ORIGIN_CC_EXP}\"></iframe>
    `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: `Two same-origin iframes`,
    fixtureData: `
      <iframe src=${SAME_ORIGIN_ALL_FIELDS}></iframe>
      <iframe src=${SAME_ORIGIN_ALL_FIELDS}></iframe>
    `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: `Two same-origin iframes, one of the iframe is sandboxed`,
    fixtureData: `
      <iframe src=${SAME_ORIGIN_ALL_FIELDS}></iframe>
      <iframe src=${SAME_ORIGIN_ALL_FIELDS} sandbox></iframe>
    `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: `Every field is in its own sandboxed same-origin iframe`,
    fixtureData: `
      <iframe src=\"${SAME_ORIGIN_CC_NUMBER}\" sandbox></iframe>
      <iframe src=\"${SAME_ORIGIN_CC_NAME}\" sandbox></iframe>
      <iframe src=\"${SAME_ORIGIN_CC_EXP}\" sandbox></iframe>
    `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: `Every field is in its own sandboxed same-origin iframe`,
    fixtureData: `
      <iframe src="https://example.com/document-builder.sjs?html=
      ${encodeURIComponent(`
        <input id="cc-number" autocomplete="cc-number">
        <input id="cc-name" autocomplete="cc-name">
        <input id="cc-exp" autocomplete="cc-exp">
        <input id="cc-number" autocomplete="cc-number">
        <input id="cc-name" autocomplete="cc-name">
        <input id="cc-exp-month" autocomplete="cc-exp-month">
        <input id="cc-exp-year" autocomplete="cc-exp-year">
      `)}"></iframe>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp-month" },
          { fieldName: "cc-exp-year" },
        ],
      },
    ],
  },
]);
