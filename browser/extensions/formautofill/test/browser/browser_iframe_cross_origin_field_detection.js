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
    description: `All fields are in the same cross-origin iframe`,
    fixtureData: `<iframe src=${CROSS_ORIGIN_ALL_FIELDS}></iframe>`,
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
    description: `Mix cross-origin and same-origin iframe`,
    fixtureData: `
      <iframe src=\"${SAME_ORIGIN_CC_NUMBER}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_NAME}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_EXP}\"></iframe>
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
    description: `Mix main-frame and cross-origin iframe`,
    fixtureData: `
      <iframe src=\"${CROSS_ORIGIN_CC_NUMBER}\"></iframe>
      <p><label>Card Name: <input id="cc-name" autocomplete="cc-name"></label></p>
      <iframe src=\"${CROSS_ORIGIN_CC_EXP}\"></iframe>
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
    description: `Mix main-frame, same-origin iframe, and cross-origin iframe`,
    fixtureData: `
      <p><label>Card Number: <input id="cc-number" autocomplete="cc-number"></label></p>
      <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_EXP}\"></iframe>
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
    description: `Every fields is in its own cross-origin iframe`,
    fixtureData: `
      <iframe src=\"${CROSS_ORIGIN_CC_NUMBER}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_NAME}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_EXP}\"></iframe>
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
    description: `One same-origin iframe and one cross-origin iframe`,
    fixtureData: `
      <iframe src=${SAME_ORIGIN_ALL_FIELDS}></iframe>
      <iframe src=${CROSS_ORIGIN_ALL_FIELDS}></iframe>
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
    description: `Mutliple cross-origin iframes`,
    fixtureData: `
      <iframe src=\"${SAME_ORIGIN_CC_NUMBER}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_NAME}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_2_CC_EXP}\"></iframe>
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
    description: `Two cross-origin iframes, one of the iframe is sandboxed`,
    fixtureData: `
      <iframe src=${CROSS_ORIGIN_ALL_FIELDS}></iframe>
      <iframe src=${CROSS_ORIGIN_ALL_FIELDS} sandbox></iframe>
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
    description: `Every field is in its own sandboxed cross-origin iframe`,
    fixtureData: `
      <iframe src=\"${CROSS_ORIGIN_CC_NUMBER}\" sandbox></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_NAME}\" sandbox></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_EXP}\" sandbox></iframe>
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
]);
