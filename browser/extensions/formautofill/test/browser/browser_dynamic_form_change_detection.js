/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.supported", "on"],
      ["extensions.formautofill.creditCards.supported", "on"],
      ["extensions.formautofill.heuristics.detectDynamicFormChanges", true],
    ],
  });
});

const verifyCurrentIdentifiedFields = async (browser, expectedFields) => {
  const actor =
    browser.browsingContext.currentWindowGlobal.getActor("FormAutofill");
  let actualFields = Array.from(actor.sectionsByRootId.values()).flat();
  verifySectionFieldDetails(actualFields, expectedFields);
};

const expectedAddressFieldsExcludingAdditionalFields = [
  {
    fields: [
      { fieldName: "name" },
      { fieldName: "email" },
      { fieldName: "tel" },
      { fieldName: "country" },
    ],
  },
];

const expectedAddressFieldsIncludingAdditionalFields = [
  {
    fields: [
      { fieldName: "name" },
      { fieldName: "email" },
      { fieldName: "tel" },
      { fieldName: "country" },
      { fieldName: "street-address" },
      { fieldName: "address-level1" },
      { fieldName: "address-level2" },
      { fieldName: "postal-code" },
    ],
  },
];

const expectedCreditCardFieldsExcludingAdditionalFields = [
  {
    fields: [{ fieldName: "cc-number" }],
  },
];

const expectedCreditCardFieldsIncludingAdditionalFields = [
  {
    fields: [
      { fieldName: "cc-number" },
      { fieldName: "cc-name" },
      { fieldName: "cc-exp-month" },
      { fieldName: "cc-exp-year" },
    ],
  },
];

/**
 * Tests that the identified fields are updated correctly during dynamic form changes
 *
 * The form changes can be of two types (see FormAutofillHeuristics.FORM_CHANGE_REASON):
 *          1. An element changes its visibility state,
 *            e.g. visible element becomes invisible or vice versa
 *          2. The form/document adds or removes nodes
 *
 * Both form changes should make FormAutofillChild consider triggering another identification process.
 * This method tests that we identifiy the correct fields in three different scenarios:
 *          1. Focusing on a field which triggers a first identification process
 *          2. Filling a field that dispatches a change event which makes the site
 *             change the form, which triggers a second field identification process
 *          3. Clearing the value from the field, which reverts the previous form change
 *             and triggers a third field identification process
 *
 * @param {string} documentPath
 * @param {string} elementIdToFill
 * @param {object[]} identifiedFieldsExcludingExtraFields
 * @param {object[]} identifiedFieldsIncludingExtraFields
 */
const verifyIdentifiedFieldsDuringFormChange = async (
  documentPath,
  elementIdToFill,
  identifiedFieldsExcludingExtraFields,
  identifiedFieldsIncludingExtraFields
) => {
  await BrowserTestUtils.withNewTab(documentPath, async browser => {
    const fieldDetectionCompletedBeforeFormChangePromise =
      getFieldDetectionCompletedPromiseResolver();

    info("Focusing on a form field to trigger the identification process");
    await SpecialPowers.spawn(browser, [elementIdToFill], elementId => {
      const field = content.document.getElementById(elementId);
      // Focus event invokes the first field identification process
      field.focus();
    });

    info("Waiting for initial fieldDetectionCompleted notification");
    await fieldDetectionCompletedBeforeFormChangePromise;

    info("Checking that additional fields are not identified yet");
    await verifyCurrentIdentifiedFields(
      browser,
      identifiedFieldsExcludingExtraFields
    );

    const fieldDetectionCompletedIncludingAdditionalFieldsPromise =
      getFieldDetectionCompletedPromiseResolver();

    info("Simulating user input so that additional field nodes are added.");
    await SpecialPowers.spawn(browser, [elementIdToFill], elementId => {
      let field = content.document.getElementById(elementId);
      field.setUserInput("dummyValue");
    });

    // A "form-changed" event was dispatched, triggering another field identification process
    info("Waiting for another fieldDetectionCompleted notification");
    await fieldDetectionCompletedIncludingAdditionalFieldsPromise;

    info("Checking that additional address fields are also identied.");
    await verifyCurrentIdentifiedFields(
      browser,
      identifiedFieldsIncludingExtraFields
    );

    const fieldDetectionCompletedExcludingAdditionalFieldsPromise =
      getFieldDetectionCompletedPromiseResolver();

    info("Clearing user input, so that additional fields are removed again.");
    await SpecialPowers.spawn(browser, [elementIdToFill], elementId => {
      let field = content.document.getElementById(elementId);
      field.focus();
      field.setUserInput("");
    });

    // A "form-changed" event was dispatched, triggering another field identification process
    info("Waiting for another fieldIdentified notification");
    await fieldDetectionCompletedExcludingAdditionalFieldsPromise;

    info("Checking that additional fields are removed from identified fields");
    await verifyCurrentIdentifiedFields(
      browser,
      identifiedFieldsExcludingExtraFields
    );
  });
};

/**
 * Tests that the correct address fields are identified in a form after "form-changed"
 * events are dispatched with reasons "nodes-added" and "nodes-removed"
 */
add_task(
  async function correct_address_fields_identified_in_form_during_form_changes_due_to_node_mutations() {
    await verifyIdentifiedFieldsDuringFormChange(
      FORMS_WITH_DYNAMIC_FORM_CHANGE,
      "country-node-addition",
      expectedAddressFieldsExcludingAdditionalFields,
      expectedAddressFieldsIncludingAdditionalFields
    );
  }
);

/**
 * Tests that the correct credit card fields are identified in a form after "form-changed"
 * events are dispatched with reasons "nodes-added" and "nodes-removed"
 */
add_task(
  async function correct_credit_card_fields_identified_in_form_during_form_changes_due_to_node_mutations() {
    await verifyIdentifiedFieldsDuringFormChange(
      FORMS_WITH_DYNAMIC_FORM_CHANGE,
      "cc-number-node-addition",
      expectedCreditCardFieldsExcludingAdditionalFields,
      expectedCreditCardFieldsIncludingAdditionalFields
    );
  }
);

/**
 * Tests that the correct address fields are identified in a form after "form-changed"
 * events are dispatched with reasons "visible-element-became-invisible" and "invisible-element-became-visible"
 */
add_task(
  async function correct_address_fields_identified_in_form_during_form_changes_due_to_element_visibility_change() {
    await verifyIdentifiedFieldsDuringFormChange(
      FORMS_WITH_DYNAMIC_FORM_CHANGE,
      "country-visibility-change",
      expectedAddressFieldsExcludingAdditionalFields,
      expectedAddressFieldsIncludingAdditionalFields
    );
  }
);

/**
 * Tests that the correct credit card fields are identified in a form after "form-changed"
 * events are dispatched with reasons "visible-element-became-invisible" and "invisible-element-became-visible"
 */
add_task(
  async function correct_credit_card_fields_identified_in_form_during_form_changes_due_to_element_visibility_change() {
    await verifyIdentifiedFieldsDuringFormChange(
      FORMS_WITH_DYNAMIC_FORM_CHANGE,
      "cc-number-visibility-change",
      expectedCreditCardFieldsExcludingAdditionalFields,
      expectedCreditCardFieldsIncludingAdditionalFields
    );
  }
);

/**
 * Tests that the correct fields are identified in a document (form-less) after "form-changed"
 * events are dispatched with reasons "nodes-added" and "nodes-removed"
 */
add_task(
  async function correct_fields_identified_in_formless_document_during_form_changes_due_to_node_mutations() {
    await verifyIdentifiedFieldsDuringFormChange(
      FORMLESS_FIELDS_WITH_DYNAMIC_FORM_CHANGE_AFTER_NODE_MUTATIONS,
      "country-node-addition",
      expectedAddressFieldsExcludingAdditionalFields,
      expectedAddressFieldsIncludingAdditionalFields
    );
  }
);

/**
 * Tests that the correct fields are identified in a document (form-less) after "form-changed"
 * events are dispatched with reasons "visible-element-became-invisible" and "invisible-element-became-visible"
 */
add_task(
  async function correct_fields_identified_in_formless_document_during_form_changes_due_to_element_visibility_change() {
    await verifyIdentifiedFieldsDuringFormChange(
      FORMLESS_FIELDS_WITH_DYNAMIC_FORM_CHANGE_AFTER_VISIBILITY_STATE_CHANGE,
      "country-visibility-change",
      expectedAddressFieldsExcludingAdditionalFields,
      expectedAddressFieldsIncludingAdditionalFields
    );
  }
);
