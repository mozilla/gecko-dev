/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This test verifies that an input with aria-invalid="true/grammar/spelling" exposes the MOX accessible for
 * its error message via AXErrorMessageElements.
 */
addAccessibleTask(
  `
  <input id="input-invalid" aria-invalid="true" aria-errormessage="error-msg">
  <input id="input-invalid-grammar" aria-invalid="grammar" aria-errormessage="error-msg">
  <input id="input-invalid-spelling" aria-invalid="spelling" aria-errormessage="error-msg">
  <div id="error-msg">Field validation failed</div>
  `,
  (_browser, accDoc) => {
    const messagesInvalid = getNativeInterface(accDoc, "input-invalid")
      .getAttributeValue("AXErrorMessageElements")
      .map(e => e.getAttributeValue("AXDOMIdentifier"));

    is(
      messagesInvalid.length,
      1,
      "Only one element referenced via 'aria-errormessage'"
    );
    is(
      messagesInvalid[0],
      "error-msg",
      "input#input-invalid refers 'error-msg' in the 'aria-errormessage'"
    );

    const messagesInvalidGrammar = getNativeInterface(
      accDoc,
      "input-invalid-grammar"
    )
      .getAttributeValue("AXErrorMessageElements")
      .map(e => e.getAttributeValue("AXDOMIdentifier"));

    is(
      messagesInvalidGrammar.length,
      1,
      "Only one element referenced via 'aria-errormessage'"
    );
    is(
      messagesInvalidGrammar[0],
      "error-msg",
      "input#input-invalid-grammar refers 'error-msg' in the 'aria-errormessage'"
    );

    const messagesInvalidSpelling = getNativeInterface(
      accDoc,
      "input-invalid-spelling"
    )
      .getAttributeValue("AXErrorMessageElements")
      .map(e => e.getAttributeValue("AXDOMIdentifier"));

    is(
      messagesInvalidSpelling.length,
      1,
      "Only one element referenced via 'aria-errormessage'"
    );
    is(
      messagesInvalidSpelling[0],
      "error-msg",
      "input#input-invalid-spelling refers 'error-msg' in the 'aria-errormessage'"
    );
  }
);

/**
 * This test verifies that an input with aria-invalid=true exposes all the MOX accessibles defined through `aria-errormessage`
 * via AXErrorMessageElements
 */
addAccessibleTask(
  `
  <label for="input">Field with error</label><input id="input" aria-invalid="true" aria-errormessage="error-msg-specialchar error-msg-10charlong">
  <div id="error-msg-specialchar">Field must contain special characters</div>
  <div id="error-msg-10charlong">Field must contain more than 10 characters</div>
  `,
  (_browser, accDoc) => {
    let input = getNativeInterface(accDoc, "input");
    const errorMessageList = input.getAttributeValue("AXErrorMessageElements");

    let messages = errorMessageList.map(e =>
      e.getAttributeValue("AXDOMIdentifier")
    );
    messages.sort();

    is(
      messages.length,
      2,
      "input#input references two elements via 'aria-errormessage'"
    );
    is(
      messages[0],
      "error-msg-10charlong",
      "We expect all elements listed in 'aria-errormessage'"
    );
    is(
      messages[1],
      "error-msg-specialchar",
      "We expect all elements listed in 'aria-errormessage'"
    );
  }
);

/**
 * When aria-invalid is set to "false", attribute is missing or without a value, AXErrorMessageElements should
 * not return associated error messages.
 * This test verifies that in this cases, AXErrorMessageElements returns `null`.
 */
addAccessibleTask(
  `
  <label for="input">Field with error</label><input id="input-invalid-false" aria-invalid="false" aria-errormessage="error-msg-specialchar error-msg-10charlong">
  <label for="input">Field with error</label><input id="input-invalid-missing" aria-errormessage="error-msg-specialchar error-msg-10charlong">
  <label for="input">Field with error</label><input id="input-invalid-spelling-error" aria-invalid aria-errormessage="error-msg-specialchar error-msg-10charlong">
  <div id="error-msg-specialchar">Field must contain special characters</div>
  <div id="error-msg-10charlong">Field must contain more than 10 characters</div>
  `,
  (_browser, accDoc) => {
    const errorsForInvalidFalse = getNativeInterface(
      accDoc,
      "input-invalid-false"
    ).getAttributeValue("AXErrorMessageElements");

    is(
      errorsForInvalidFalse,
      null,
      "When aria-invalid is 'false', [AXErrorMessageElements] should return null"
    );

    const errorsForInvalidMissing = getNativeInterface(
      accDoc,
      "input-invalid-missing"
    ).getAttributeValue("AXErrorMessageElements");

    is(
      errorsForInvalidMissing,
      null,
      "When aria-invalid is missing, [AXErrorMessageElements] should return null"
    );

    const errorsForSpellingError = getNativeInterface(
      accDoc,
      "input-invalid-spelling-error"
    ).getAttributeValue("AXErrorMessageElements");

    is(
      errorsForSpellingError,
      null,
      "When aria-invalid is provided without value, [AXErrorMessageElements] should return null"
    );
  }
);

/**
 * This test modifies the innerText of an associated error message and verifies the correct event AXValidationErrorChagned is fired.
 */
addAccessibleTask(
  `
    <label for="input">Field with error</label><input id="input" aria-invalid="true" aria-errormessage="error-msg">
    <div id="error-msg">Field validation failed</div>
  `,
  async (browser, _accDoc) => {
    let validationErrorChanged = waitForMacEvent("AXValidationErrorChanged");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("error-msg").innerText =
        "new error message";
    });
    await validationErrorChanged;
    info("validationErrorChanged: event has arrived");
  }
);

/**
 * This test modifies the inner tree of an associated error message and verifies the correct event AXValidationErrorChagned is fired.
 */
addAccessibleTask(
  `
    <label for="input">Field with error</label><input id="input" aria-invalid="true" aria-errormessage="error-msg">
    <div id="error-msg">Field validation failed <span id="inner-error-msg"></span></div>
  `,
  async (browser, _accDoc) => {
    let validationErrorChanged = waitForMacEvent("AXValidationErrorChanged");

    info("validationErrorChanged: changing inner element");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("inner-error-msg").innerText =
        "detailed error message";
    });

    await validationErrorChanged;

    info("validationErrorChanged: event has arrived");
  }
);

/**
 * When the value of `aria-errormessage` is changed, AXValidationErrorChanged should be triggered.
 * The test removes the element id from `aria-errormessage` and checks that:
 * - the event was fired
 * - AXErrorMessageElements does not return error messages
 *
 * Then, the test inserts element id back to `aria-errormessage` and checks that:
 * - the event AXValidationErrorChanged was fired
 * - AXErrorMessageElements contain our error message
 */
addAccessibleTask(
  `
    <label for="input">Field with error</label><input id="input" aria-invalid="true" aria-errormessage="error-msg">
    <div id="error-msg">Field validation failed</div>
  `,
  async (browser, accDoc) => {
    let validationErrorChanged = waitForMacEvent("AXValidationErrorChanged");

    info("validationErrorChanged: removing reference to error");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("input")
        .setAttribute("aria-errormessage", "");
    });

    await validationErrorChanged;

    info("validationErrorChanged: event has arrived");

    let validationErrors = getNativeInterface(
      accDoc,
      "input"
    ).getAttributeValue("AXErrorMessageElements");

    is(
      validationErrors,
      null,
      "We have removed reference to error message, AXErrorMessageElements should now contain nothing"
    );

    info("validationErrorChanged: adding the reference back");
    validationErrorChanged = waitForMacEvent("AXValidationErrorChanged");

    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("input")
        .setAttribute("aria-errormessage", "error-msg");
    });

    await validationErrorChanged;

    validationErrors = getNativeInterface(accDoc, "input")
      .getAttributeValue("AXErrorMessageElements")
      .map(e => e.getAttributeValue("AXDOMIdentifier"));

    info("validation errors: " + JSON.stringify(validationErrors));

    is(
      validationErrors.length,
      1,
      "Reference to 'error-msg' was returned back"
    );
    is(
      validationErrors[0],
      "error-msg",
      "Reference to 'error-msg' was returned back"
    );
  }
);

/**
 * This test modifies the innerText of an associated error message on an
 * input with aria-invalid=false and verifies the error change event
 * is NOT fired.
 */
addAccessibleTask(
  `
    <label for="input">Valid field with associated error error</label><input id="input" aria-invalid="false" aria-errormessage="error-msg">
    <div id="error-msg">Field validation failed</div>
  `,
  async (browser, _accDoc) => {
    // XXX: We don't have a way to await unexpected, non-core events, so we
    // use the core EVENT_ERRORMESSAGE_CHANGED here as a proxy for AXValidationErrorChanged
    const unexpectedEvents = { unexpected: [[EVENT_ERRORMESSAGE_CHANGED]] };
    info("Setting new error message text");
    await contentSpawnMutation(browser, unexpectedEvents, function () {
      content.document.getElementById("error-msg").innerText =
        "new error message";
    });
    ok(true, "Did not receive error message event!");
  }
);
