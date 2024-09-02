/* global add_capture_heuristic_tests */

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.creditCards.supported", "on"],
      ["extensions.formautofill.creditCards.enabled", true],
      ["extensions.formautofill.addresses.capture.requiredFields", ""],
      ["extensions.formautofill.loglevel", "Debug"],
    ],
  });
});

const CAPTURE_FILL_VALUE = {
  "#cc-number": "4111111111111111",
  "#cc-name": "John Doe",
  "#cc-exp": `04/${new Date().getFullYear()}`,
};

const CAPTURE_EXPECTED_RECORD = {
  "cc-number": "************1111",
  "cc-name": "John Doe",
  "cc-exp-month": 4,
  "cc-exp-year": new Date().getFullYear(),
  "cc-type": "visa",
};

const CAPTURE_FILL_VALUE_1 = {
  "#form1 #cc-number": "378282246310005",
  "#form1 #cc-name": "Timothy Berners-Lee",
  "#form1 #cc-exp": `07/${new Date().getFullYear() - 1}`,
};

const CAPTURE_EXPECTED_RECORD_1 = {
  "cc-number": "***********0005",
  "cc-name": "Timothy Berners-Lee",
  "cc-exp-month": 7,
  "cc-exp-year": new Date().getFullYear() - 1,
  "cc-type": "amex",
};

const CAPTURE_FILL_VALUE_2 = {
  "#form2 #cc-number": "5555555555554444",
  "#form2 #cc-name": "Jane Doe",
  "#form2 #cc-exp": `12/${new Date().getFullYear() + 1}`,
};

const CAPTURE_EXPECTED_RECORD_2 = {
  "cc-number": "************4444",
  "cc-name": "Jane Doe",
  "cc-exp-month": 12,
  "cc-exp-year": new Date().getFullYear() + 1,
  "cc-type": "mastercard",
};

add_capture_heuristic_tests([
  {
    description: `All fields are in the same same-origin iframe`,
    fixtureData: `
      <form id="form1">
        <iframe src=${SAME_ORIGIN_ALL_FIELDS}></iframe>
        <input id="submit" type="submit">
      </form>
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
    captureFillValue: CAPTURE_FILL_VALUE,
    captureExpectedRecord: CAPTURE_EXPECTED_RECORD,
  },
  {
    description: `All fields are in the same cross-origin iframe`,
    fixtureData: `
      <form id="form1">
        <iframe src=${CROSS_ORIGIN_ALL_FIELDS}></iframe>
        <input id="submit" type="submit">
      </form>
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
    captureFillValue: CAPTURE_FILL_VALUE,
    captureExpectedRecord: CAPTURE_EXPECTED_RECORD,
  },
  {
    description:
      "One main-frame, one same-origin iframe and one cross-origin iframe",
    fixtureData: `
      <form id="form1">
        <p><label>Card Number: <input id="cc-number" autocomplete="cc-number"></label></p>
        <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
        <iframe src=\"${CROSS_ORIGIN_CC_EXP}\"></iframe>
        <input id="submit" type="submit">
      </form>
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
    captureFillValue: CAPTURE_FILL_VALUE,
    captureExpectedRecord: CAPTURE_EXPECTED_RECORD,
  },
  {
    description: `Every field is in its own same-origin iframe`,
    fixtureData: `
      <form id="form1">
        <iframe src=\"${SAME_ORIGIN_CC_NUMBER}\"></iframe>
        <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
        <iframe src=\"${SAME_ORIGIN_CC_EXP}\"></iframe>
        <input id="submit" type="submit">
      </form>
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
    captureFillValue: CAPTURE_FILL_VALUE,
    captureExpectedRecord: CAPTURE_EXPECTED_RECORD,
  },
  {
    description: `Every field is in its own corss-origin iframe`,
    fixtureData: `
      <form id="form1">
        <iframe src=\"${CROSS_ORIGIN_CC_NUMBER}\"></iframe>
        <iframe src=\"${CROSS_ORIGIN_CC_NAME}\"></iframe>
        <iframe src=\"${CROSS_ORIGIN_CC_EXP}\"></iframe>
        <input id="submit" type="submit">
      </form>
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
    captureFillValue: CAPTURE_FILL_VALUE,
    captureExpectedRecord: CAPTURE_EXPECTED_RECORD,
  },
  {
    description: `Two forms, submit the cross-origin form`,
    fixtureData: `
      <form>
        <iframe src=${SAME_ORIGIN_ALL_FIELDS}?formId=form1></iframe>
      </form>
      <form>
        <iframe src=${CROSS_ORIGIN_ALL_FIELDS}?formId=form2></iframe>
      </form>
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
    submitButtonSelector: "#form2 input[type=submit]",
    captureFillValue: { ...CAPTURE_FILL_VALUE_1, ...CAPTURE_FILL_VALUE_2 },
    captureExpectedRecord: CAPTURE_EXPECTED_RECORD_2,
  },
  {
    description: `Two forms, submit the same-origin form`,
    fixtureData: `
      <form>
        <iframe src=${SAME_ORIGIN_ALL_FIELDS}?formId=form1></iframe>
      </form>
      <form>
        <iframe src=${CROSS_ORIGIN_ALL_FIELDS}?formId=form2></iframe>
      </form>
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
    submitButtonSelector: "#form1 input[type=submit]",
    captureFillValue: { ...CAPTURE_FILL_VALUE_1, ...CAPTURE_FILL_VALUE_2 },
    captureExpectedRecord: CAPTURE_EXPECTED_RECORD_1,
  },
  {
    description:
      "One main-frame, one same-origin sandbox iframe and one cross-origin sandbox iframe",
    fixtureData: `
      <form id="form1">
        <p><label>Card Number: <input id="cc-number" autocomplete="cc-number"></label></p>
        <iframe src=\"${SAME_ORIGIN_CC_NAME}\" sandbox></iframe>
        <iframe src=\"${CROSS_ORIGIN_CC_EXP}\" sandbox></iframe>
        <input id="submit" type="submit">
      </form>
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
    captureFillValue: CAPTURE_FILL_VALUE,
    captureExpectedRecord: CAPTURE_EXPECTED_RECORD,
  },
]);
