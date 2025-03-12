/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FormAutofill } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofill.sys.mjs"
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

  registerCleanupFunction(async () => {
    await removeAllRecords();
    FormAutofillUtils.setOSAuthEnabled(
      FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF,
      oldValue
    );
  });
});

const expectedFilledCreditCardFields = {
  fields: [
    { fieldName: "cc-number", autofill: TEST_CREDIT_CARD_1["cc-number"] },
    { fieldName: "cc-name", autofill: TEST_CREDIT_CARD_1["cc-name"] },
    { fieldName: "cc-exp-month", autofill: TEST_CREDIT_CARD_1["cc-exp-month"] },
    { fieldName: "cc-exp-year", autofill: TEST_CREDIT_CARD_1["cc-exp-year"] },
  ],
};

const expectedFilledAddressFields = {
  fields: [
    { fieldName: "name", autofill: "John R. Smith" },
    { fieldName: "email", autofill: TEST_ADDRESS_1.email },
    { fieldName: "tel", autofill: TEST_ADDRESS_1.tel },
    { fieldName: "country", autofill: TEST_ADDRESS_1.country },
    {
      fieldName: "street-address",
      autofill: TEST_ADDRESS_1["street-address"].replace("\n", " "),
    },
    { fieldName: "address-level1", autofill: TEST_ADDRESS_1["address-level1"] },
    { fieldName: "address-level2", autofill: TEST_ADDRESS_1["address-level2"] },
    { fieldName: "postal-code", autofill: TEST_ADDRESS_1["postal-code"] },
  ],
};

/**
 * Verify that fields that are added/become visible immediately after
 * an initial autocompletion get filled as well
 *
 * @param {string} documentPath
 * @param {string} selectorToTriggerAutocompletion
 */
const verifyAutofilledFieldsDuringFormChange = async (
  documentPath,
  selectorToTriggerAutocompletion,
  elementValueToVerifyAutofill,
  expectedSection
) => {
  await BrowserTestUtils.withNewTab(documentPath, async browser => {
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

    info("Verifying that all fields are filled correctly.");
    const actor =
      browser.browsingContext.currentWindowGlobal.getActor("FormAutofill");
    const section = Array.from(actor.sectionsByRootId.values()).flat()[0];
    await verifyAutofillResult(browser, section, expectedSection);
  });
};

/**
 * Tests that all address fields are filled.
 * Form adds autofillable input fields after country field is modified
 */
add_task(
  async function address_fields_filled_in_form_during_form_changes_due_to_node_mutations() {
    await verifyAutofilledFieldsDuringFormChange(
      FORMS_WITH_DYNAMIC_FORM_CHANGE,
      "#country-node-addition",
      TEST_ADDRESS_1.country,
      expectedFilledAddressFields
    );
  }
);

/**
 * Tests that all credit card fields are filled.
 * Form adds autofillable input fields after cc number field is modified.
 */
add_task(
  async function credit_card_fields_filled_in_form_during_form_changes_due_to_node_mutations() {
    await verifyAutofilledFieldsDuringFormChange(
      FORMS_WITH_DYNAMIC_FORM_CHANGE,
      "#cc-number-node-addition",
      TEST_CREDIT_CARD_1["cc-number"],
      expectedFilledCreditCardFields
    );
  }
);

/**
 * Tests that all address fields are filled.
 * Form makes invisible autofillable input fields become visible after country field is modified
 */
add_task(
  async function address_fields_filled_in_form_during_form_changes_due_to_element_visibility_change() {
    await verifyAutofilledFieldsDuringFormChange(
      FORMS_WITH_DYNAMIC_FORM_CHANGE,
      "#country-visibility-change",
      TEST_ADDRESS_1.country,
      expectedFilledAddressFields
    );
  }
);

/**
 * Tests that all credit card fields are filled.
 * Form makes invisible autofillable input fields become visible after cc number field is modified.
 */
add_task(
  async function credit_card_fields_filled_in_form_during_form_changes_due_to_element_visibility_change() {
    await verifyAutofilledFieldsDuringFormChange(
      FORMS_WITH_DYNAMIC_FORM_CHANGE,
      "#cc-number-visibility-change",
      TEST_CREDIT_CARD_1["cc-number"],
      expectedFilledCreditCardFields
    );
  }
);

/**
 * Tests that all fields are filled.
 * Formless document adds autofillable input fields after country field is modified.
 */
add_task(
  async function address_fields_filled_in_formless_document_during_form_changes_due_to_node_mutations() {
    await verifyAutofilledFieldsDuringFormChange(
      FORMLESS_FIELDS_WITH_DYNAMIC_FORM_CHANGE_AFTER_NODE_MUTATIONS,
      "#country-node-addition",
      TEST_ADDRESS_1.country,
      expectedFilledAddressFields
    );
  }
);

/**
 * Tests that all fields are filled.
 * Formless document makes invisible autofillable input fields become visible after country field is modified.
 */
add_task(
  async function address_fields_filled_in_formless_document_during_form_changes_due_to_element_visibility_change() {
    await verifyAutofilledFieldsDuringFormChange(
      FORMLESS_FIELDS_WITH_DYNAMIC_FORM_CHANGE_AFTER_VISIBILITY_STATE_CHANGE,
      "#country-visibility-change",
      TEST_ADDRESS_1.country,
      expectedFilledAddressFields
    );
  }
);

/**
 * Tests that additional fields are not filled when the form change was initiated
 * by a user interaction that triggered a "click" event on the form.
 */
add_task(
  async function additional_fields_not_filled_on_user_initiated_form_change() {
    await BrowserTestUtils.withNewTab(
      FORM_WITH_USER_INITIATED_FORM_CHANGE,
      async browser => {
        const selectorToTriggerAutocompletion = "#country-visibility-change";
        const elementValueToVerifyAutofill = TEST_ADDRESS_1.country;

        info("Triggering autocompletion.");
        await openPopupOn(browser, selectorToTriggerAutocompletion);
        await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
        await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
        await waitForAutofill(
          browser,
          selectorToTriggerAutocompletion,
          elementValueToVerifyAutofill
        );

        info(
          "Simulating user interaction to cancel any filling on dynamic form change actions"
        );
        const showFieldButtonSelector = "#show-fields-btn";
        await SpecialPowers.spawn(
          browser,
          [showFieldButtonSelector],
          async btnSelector => {
            const showFieldsButton =
              content.document.querySelector(btnSelector);
            showFieldsButton.click();
          }
        );

        info(
          "Waiting for any possible filling on dynamic form change to complete"
        );
        /* eslint-disable mozilla/no-arbitrary-setTimeout */
        await new Promise(resolve => {
          setTimeout(resolve, FormAutofill.fillOnDynamicFormChangeTimeout);
        });

        info(
          "Verifying that all fields are detected, but additional fields are not filled"
        );
        const expectedAdditionalFieldsNotFilled = {
          fields: [
            { fieldName: "name", autofill: "John R. Smith" },
            { fieldName: "email", autofill: TEST_ADDRESS_1.email },
            { fieldName: "tel", autofill: TEST_ADDRESS_1.tel },
            { fieldName: "country", autofill: TEST_ADDRESS_1.country },
            {
              fieldName: "street-address",
            },
            { fieldName: "address-level1" },
            { fieldName: "address-level2" },
            { fieldName: "postal-code" },
          ],
        };
        const actor =
          browser.browsingContext.currentWindowGlobal.getActor("FormAutofill");
        const section = Array.from(actor.sectionsByRootId.values()).flat()[0];
        await verifyAutofillResult(
          browser,
          section,
          expectedAdditionalFieldsNotFilled
        );
      }
    );
  }
);
