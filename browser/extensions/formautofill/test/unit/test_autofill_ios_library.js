/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FormAutofillChild } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofillChild.ios.sys.mjs"
);

const { AutofillTelemetry } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/AutofillTelemetry.sys.mjs"
);

var { FormAutofillHandler } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/FormAutofillHandler.sys.mjs"
);

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
  {
    description: `Test correct street-address to address-line1`,
    document: `<form>
                 <input id="email" autocomplete="email">
                 <input id="tel" autocomplete="tel">
                 <input id="street-address" >
                 <input id="address-line2">
                <form>`,
    fillPayload: {
      "street-address": "2 Harrison St\nline2",
      email: "foo@mozilla.com",
      tel: "1234567",
    },

    expectedFill: {
      "#street-address": "2 Harrison St",
      "#address-line2": "line2",
      "#email": "foo@mozilla.com",
      "#tel": "1234567",
    },

    expectedDetectedFields: {
      email: "",
      tel: "",
      "address-line1": "",
      "address-line2": "",
    },

    expectedSubmit: null,
  },
];

const recordFormInteractionEventStub = sinon.stub(
  AutofillTelemetry,
  "recordFormInteractionEvent"
);

add_setup(() => {
  registerCleanupFunction(() => sinon.restore());
});

add_task(async function test_ios_api() {
  for (const TEST of TEST_CASES) {
    info(`Test ${TEST.description}`);
    const doc = MockDocument.createTestDocument(
      "http://localhost:8080/test/",
      TEST.document
    );

    const autofillSpy = sinon.spy();
    const submitSpy = sinon.spy();

    const fac = new FormAutofillChild({
      address: {
        autofill: autofillSpy,
        submit: submitSpy,
      },
      creditCard: {
        autofill: autofillSpy,
        submit: submitSpy,
      },
    });

    // Test `onFocusIn` API
    fac.onFocusIn({ target: doc.querySelector("input") });
    Assert.ok(
      autofillSpy.calledOnce,
      "autofill callback should be called once"
    );
    Assert.ok(
      autofillSpy.calledWithExactly(TEST.expectedDetectedFields),
      "autofill callback should be called with correct payload"
    );
    Assert.ok(
      recordFormInteractionEventStub.calledWithMatch("detected"),
      "detect telemetry event should be recorded"
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
    Assert.ok(
      recordFormInteractionEventStub.calledWithMatch("filled"),
      "filled telemetry event should be recorded"
    );

    // Test `onFilledModified` API
    Object.entries(TEST.expectedFill).forEach(([selector, expectedValue]) => {
      const element = doc.querySelector(selector);
      // Simulate input change (e.g. adding a char)
      FormAutofillHandler.fillFieldValue(element, expectedValue + "a");
      Assert.ok(
        recordFormInteractionEventStub.calledWithMatch("filled_modified"),
        "filled_modified telemetry event should be recorded"
      );
      FormAutofillHandler.fillFieldValue(element, expectedValue);
    });

    // Test `onSubmit` API
    if (TEST.expectedSubmit) {
      fac.onSubmit();
      Assert.ok(submitSpy.calledOnce, "submit callback should be called once");
      Assert.ok(
        submitSpy.calledWithExactly(TEST.expectedSubmit),
        "submit callback should be called with correct payload"
      );
      Assert.ok(
        recordFormInteractionEventStub.calledWithMatch("submitted"),
        "submitted telemetry event should be recorded"
      );
    }
  }
});
