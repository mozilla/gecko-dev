/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FormAutofill } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofill.sys.mjs"
);
const { AddressTelemetry } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/AutofillTelemetry.sys.mjs"
);

const supportedCcFields = [
  "cc_type",
  "cc_number",
  "cc_exp_month",
  "cc_exp",
  "cc_exp_year",
  "cc_name",
];
const supportedAddressFields = AddressTelemetry.SUPPORTED_FIELDS_IN_FORM.concat(
  AddressTelemetry.SUPPORTED_FIELDS_IN_FORM_EXT
);

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.supported", "on"],
      ["extensions.formautofill.creditCards.supported", "on"],
      ["extensions.formautofill.heuristics.detectDynamicFormChanges", true],
      ["extensions.formautofill.heuristics.fillOnDynamicFormChanges", true],
      [
        "extensions.formautofill.heuristics.fillOnDynamicFormChanges.timeout",
        1000,
      ],
    ],
  });
  const oldValue = FormAutofillUtils.getOSAuthEnabled(
    FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF
  );
  FormAutofillUtils.setOSAuthEnabled(
    FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF,
    false
  );

  await setStorage(TEST_ADDRESS_1);
  await setStorage(TEST_CREDIT_CARD_1);

  await clearGleanTelemetry();

  registerCleanupFunction(async () => {
    await removeAllRecords();
    FormAutofillUtils.setOSAuthEnabled(
      FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF,
      oldValue
    );
    await clearGleanTelemetry();
  });
});

const CASE = { CREDIT_CARD: "credit_card", ADDRESS: "address" };

function buildFormEventExtra(supportedFields, extra, defaultValue) {
  let defaults = {};
  for (const field of supportedFields) {
    if (!extra[field]) {
      defaults[field] = defaultValue;
    }
  }

  return { ...defaults, ...extra };
}

const verifyFormInteractionEventsOnFieldsUpdate = async (
  documentPath,
  formautofillCase,
  selectorToTriggerAutocompletion,
  expectedEvents
) => {
  await BrowserTestUtils.withNewTab(documentPath, async browser => {
    const elementValueToVerifyAutofill =
      formautofillCase == CASE.CREDIT_CARD
        ? TEST_CREDIT_CARD_1["cc-number"]
        : TEST_ADDRESS_1.country;

    info("Triggering autocompletion.");
    await openPopupOn(browser, selectorToTriggerAutocompletion);
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);

    const filledOnFormChangePromise = TestUtils.topicObserved(
      "formautofill-fill-after-form-change-complete"
    );

    await waitForAutofill(
      browser,
      selectorToTriggerAutocompletion,
      elementValueToVerifyAutofill
    );
    info(
      `Waiting for "formautofill-fill-after-form-change-complete" notification`
    );
    await filledOnFormChangePromise;
  });

  info("Verifying recorded form interaction events");
  let flowIds = new Set();
  expectedEvents.forEach(expectedEvent => {
    const actualEvents =
      formautofillCase == CASE.CREDIT_CARD
        ? (Glean.creditcard[expectedEvent.name].testGetValue() ?? [])
        : (Glean.address[expectedEvent.name].testGetValue() ?? []);
    Assert.equal(
      actualEvents.length,
      expectedEvent.count,
      `Expected to have ${expectedEvent.count} event/s with the name "${expectedEvent.name}"`
    );
    if (expectedEvent.extra) {
      actualEvents.forEach(actualEvent => {
        const actualExtra = actualEvent.extra;
        flowIds.add(actualExtra.value);
        delete actualExtra.value;

        Assert.deepEqual(actualExtra, expectedEvent.extra);
      });
    }
  });
};

/**
 * Tests the form interaction telemetry recorded during the autocompletion of a dynamic address form,
 * meaning the form updates its fields during autocompletion and we re-fill the added/updated fields.
 */
add_task(
  async function filledOnFieldsUpdateAddressForm_event_recorded_with_correct_extra_keys_during_autocompletion_of_dynamically_changing_address_form() {
    const expectedAddressFormEvents = [
      {
        name: "detectedAddressForm",
        count: 2,
      },
      {
        name: "detectedAddressFormExt",
        count: 2,
      },
      // Ignoring popupShownCcFormV2 event
      {
        name: "filledAddressForm",
        extra: buildFormEventExtra(
          AddressTelemetry.SUPPORTED_FIELDS_IN_FORM,
          {
            country: "filled",
          },
          "unavailable"
        ),
        count: 1,
      },
      {
        name: "filledAddressFormExt",
        extra: buildFormEventExtra(
          AddressTelemetry.SUPPORTED_FIELDS_IN_FORM_EXT,
          {
            name: "filled",
            email: "filled",
            tel: "filled",
          },
          "unavailable"
        ),
        count: 1,
      },
      {
        name: "filledOnFieldsUpdateAddressForm",
        extra: buildFormEventExtra(
          supportedAddressFields,
          {
            name: "filled",
            email: "filled",
            tel: "filled",
            country: "filled",
            street_address: "filled_on_fields_update",
            address_level1: "filled_on_fields_update",
            address_level2: "filled_on_fields_update",
            postal_code: "filled_on_fields_update",
          },
          "unavailable"
        ),
        count: 1,
      },
    ];
    await verifyFormInteractionEventsOnFieldsUpdate(
      FORMS_WITH_DYNAMIC_FORM_CHANGE,
      CASE.ADDRESS,
      "#country-node-addition",
      expectedAddressFormEvents
    );
  }
);

/**
 * Tests the form interaction telemetry recorded during the autocompletion of a dynamic credit card form,
 * meaning the form updates its fields during autocompletion and we re-fill the added/updated fields.
 */
add_task(
  async function filledOnFieldsUpdateAddressForm_event_recorded_with_correct_extra_keys_during_autocompletion_of_dynamically_changing_credit_card_form() {
    const expectedCcFormEvents = [
      {
        name: "detectedCcFormV2",
        count: 2,
      },
      // Ignoring popupShownCcFormV2 event
      {
        name: "filledCcFormV2",
        extra: buildFormEventExtra(
          supportedCcFields,
          { cc_number: "filled" },
          "unavailable"
        ),
        count: 1,
      },
      {
        name: "filledOnFieldsUpdateCcFormV2",
        extra: buildFormEventExtra(
          supportedCcFields,
          {
            cc_number: "filled",
            cc_name: "filled_on_fields_update",
            cc_exp_month: "filled_on_fields_update",
            cc_exp_year: "filled_on_fields_update",
          },
          "unavailable"
        ),
        count: 1,
      },
    ];
    await verifyFormInteractionEventsOnFieldsUpdate(
      FORMS_WITH_DYNAMIC_FORM_CHANGE,
      CASE.CREDIT_CARD,
      "#cc-number-node-addition",
      expectedCcFormEvents
    );
  }
);
