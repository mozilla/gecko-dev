"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/extensions/formautofill/test/browser/creditCard/browser_telemetry_utils.js",
  this
);

add_setup(async function () {
  await clearGleanTelemetry();
});

add_task(async function test_submit_creditCard_new() {
  async function test_per_command(command, idx, expectChanged = undefined) {
    await SpecialPowers.pushPrefEnv({
      set: [[ENABLED_AUTOFILL_CREDITCARDS_PREF, true]],
    });
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: CREDITCARD_FORM_URL },
      async function (browser) {
        let onPopupShown = waitForPopupShown();
        let onChanged;
        if (expectChanged !== undefined) {
          onChanged = TestUtils.topicObserved("formautofill-storage-changed");
        }

        await focusUpdateSubmitForm(browser, {
          focusSelector: "#cc-name",
          newValues: {
            "#cc-name": "User 1",
            "#cc-number": "5038146897157463",
            "#cc-exp-month": "12",
            "#cc-exp-year": "2017",
            "#cc-type": "mastercard",
          },
        });

        await onPopupShown;
        await clickDoorhangerButton(command, idx);
        if (expectChanged !== undefined) {
          await onChanged;
          TelemetryTestUtils.assertScalar(
            TelemetryTestUtils.getProcessScalars("parent"),
            "formautofill.creditCards.autofill_profiles_count",
            expectChanged,
            `There should be ${expectChanged} profile(s) stored and recorded in Legacy Telemetry.`
          );
          Assert.equal(
            expectChanged,
            Glean.formautofillCreditcards.autofillProfilesCount.testGetValue(),
            `There should be ${expectChanged} profile(s) stored and recorded in Glean.`
          );
        }

        // flushing Glean data within withNewTab callback before tab removal (see Bug 1843178)
        await Services.fog.testFlushAllChildren();
      }
    );

    await removeAllRecords();
    await SpecialPowers.popPrefEnv();
  }

  let expectedFormInteractionEvents = [
    ccFormArgsv2("detected", buildccFormv2Extra({ cc_exp: "false" }, "true")),
    ccFormArgsv2(
      "submitted",
      buildccFormv2Extra({ cc_exp: "unavailable" }, "user_filled")
    ),
  ];

  await test_per_command(MAIN_BUTTON, undefined, 1);

  await assertTelemetry(undefined, [
    ...expectedFormInteractionEvents,
    ["creditcard", "show", "capture_doorhanger"],
    ["creditcard", "save", "capture_doorhanger"],
  ]);

  assertFormInteractionEventsInGlean(expectedFormInteractionEvents);
  assertDetectedCcNumberFieldsCountInGlean([
    { label: "cc_number_fields_1", count: 1 },
  ]);

  await clearGleanTelemetry();

  await test_per_command(SECONDARY_BUTTON);

  await assertTelemetry(undefined, [
    ...expectedFormInteractionEvents,
    ["creditcard", "show", "capture_doorhanger"],
    ["creditcard", "cancel", "capture_doorhanger"],
  ]);

  assertFormInteractionEventsInGlean(expectedFormInteractionEvents);
  assertDetectedCcNumberFieldsCountInGlean([
    { label: "cc_number_fields_1", count: 1 },
  ]);

  await clearGleanTelemetry();

  await test_per_command(MENU_BUTTON, 0);

  await assertTelemetry(undefined, [
    ...expectedFormInteractionEvents,
    ["creditcard", "show", "capture_doorhanger"],
    ["creditcard", "disable", "capture_doorhanger"],
  ]);

  assertFormInteractionEventsInGlean(expectedFormInteractionEvents);
  assertDetectedCcNumberFieldsCountInGlean([
    { label: "cc_number_fields_1", count: 1 },
  ]);
});
