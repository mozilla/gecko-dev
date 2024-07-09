/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

const { FormAutofillChild } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofillChild.ios.sys.mjs"
);

("use strict");

class Callback {
  waitForCallback() {
    this.promise = new Promise(resolve => {
      this._resolve = resolve;
    });
    return this.promise;
  }

  address = {
    autofill: fieldsWithValues => this._resolve?.(fieldsWithValues),
    submit: records => this._resolve?.(records),
  };

  creditCard = {
    autofill: fieldsWithValues => this._resolve?.(fieldsWithValues),
    submit: records => this._resolve?.(records),
  };
}

const TEST_CASES = [
  {
    description: `basic credit card form`,
    document: `<form>
                 <input id="cc-number" autocomplete="cc-number">
                 <input id="cc-name" autocomplete="cc-name">
                 <input id="cc-exp-month" autocomplete="cc-exp-month">
                 <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    fillPayload: {
      "cc-number": "4111111111111111",
      "cc-name": "test name",
      "cc-exp-month": 6,
      "cc-exp-year": 25,
    },

    expectedDetectedFields: {
      "cc-number": "",
      "cc-name": "",
      "cc-exp-month": "",
      "cc-exp-year": "",
      "cc-type": "",
    },

    expectedFill: {
      "#cc-number": "4111111111111111",
      "#cc-name": "test name",
      "#cc-exp-month": 6,
      "#cc-exp-year": 25,
    },

    expectedSubmit: [
      {
        "cc-number": "4111111111111111",
        "cc-name": "test name",
        "cc-exp-month": 6,
        "cc-exp-year": 2025,
        "cc-type": "visa",
      },
    ],
  },
  {
    description: `basic address form`,
    document: `<form>
                 <input id="given-name" autocomplete="given-name">
                 <input id="family-name" autocomplete="family-name">
                 <input id="street-address" autocomplete="street-address">
                 <input id="address-level2" autocomplete="address-level2">
                 <select id="country" autocomplete="country">
                   <option/>
                   <option value="US">United States</option>
                 </select>
                 <input id="email" autocomplete="email">
                 <input id="tel" autocomplete="tel">
                <form>`,
    fillPayload: {
      "street-address": "2 Harrison St line2",
      "address-level2": "San Francisco",
      country: "US",
      email: "foo@mozilla.com",
      tel: "1234567",
    },

    expectedFill: {
      "#street-address": "2 Harrison St line2",
      "#address-level2": "San Francisco",
      "#country": "US",
      "#email": "foo@mozilla.com",
      "#tel": "1234567",
    },

    expectedDetectedFields: {
      "given-name": "",
      "family-name": "",
      "street-address": "",
      "address-level2": "",
      country: "",
      email: "",
      tel: "",
    },

    expectedSubmit: null,
  },
];

add_task(async function test_ios_api() {
  for (const TEST of TEST_CASES) {
    info(`Test ${TEST.description}`);
    const doc = MockDocument.createTestDocument(
      "http://localhost:8080/test/",
      TEST.document
    );

    const callbacks = new Callback();
    const fac = new FormAutofillChild(callbacks);

    // Test `onFocusIn` API
    let promise = callbacks.waitForCallback();
    fac.onFocusIn({ target: doc.querySelector("input") });
    const autofillCallbackResult = await promise;

    Assert.deepEqual(
      autofillCallbackResult,
      TEST.expectedDetectedFields,
      "Should receive autofill callback"
    );

    // Test `fillFormFields` API
    fac.fillFormFields(TEST.fillPayload);
    Object.entries(TEST.expectedFill).forEach(([selector, expectedValue]) => {
      const element = doc.querySelector(selector);
      Assert.equal(
        element.value,
        expectedValue,
        `Should fill ${element.id} field correctly`
      );
    });

    // Test `onSubmit` API
    if (TEST.expectedSubmit) {
      promise = callbacks.waitForCallback();
      fac.onSubmit();
      const submitCallbackResult = await promise;

      Assert.deepEqual(
        submitCallbackResult,
        TEST.expectedSubmit,
        "Should receive submit callback"
      );
    }
  }
});
