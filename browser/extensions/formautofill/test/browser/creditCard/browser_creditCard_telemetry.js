"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const CC_NUM_USES_HISTOGRAM = "CREDITCARD_NUM_USES";

function ccFormArgsv2(method, extra) {
  return ["creditcard", method, "cc_form_v2", undefined, extra];
}

function buildccFormv2Extra(extra, defaultValue) {
  let defaults = {};
  for (const field of [
    "cc_name",
    "cc_number",
    "cc_type",
    "cc_exp",
    "cc_exp_month",
    "cc_exp_year",
  ]) {
    defaults[field] = defaultValue;
  }

  return { ...defaults, ...extra };
}

function assertDetectedCcNumberFieldsCountInGlean(expectedLabeledCounts) {
  expectedLabeledCounts.forEach(expected => {
    const actualCount =
      Glean.creditcard.detectedCcNumberFieldsCount[
        expected.label
      ].testGetValue();
    Assert.equal(
      actualCount,
      expected.count,
      `Expected counter to be ${expected.count} for label ${expected.label} - but got ${actualCount}`
    );
  });
}

function assertFormInteractionEventsInGlean(events) {
  const eventCount = 1;
  let flowIds = new Set();
  events.forEach(event => {
    const expectedName = event[1];
    const expectedExtra = event[4];
    const eventMethod = expectedName.replace(/(_[a-z])/g, c =>
      c[1].toUpperCase()
    );
    const actualEvents =
      Glean.creditcard[eventMethod + "CcFormV2"].testGetValue() ?? [];

    Assert.equal(
      actualEvents.length,
      eventCount,
      `Expected to have ${eventCount} event/s with the name "${expectedName}"`
    );

    if (expectedExtra) {
      let actualExtra = actualEvents[0].extra;
      // We don't want to test the flow_id of the form interaction session just yet
      flowIds.add(actualExtra.value);
      delete actualExtra.value;

      Assert.deepEqual(actualEvents[0].extra, expectedExtra);
    }
  });

  Assert.equal(
    flowIds.size,
    1,
    `All events from the same user interaction session have the same flow id`
  );
}

async function assertTelemetry(expected_content, expected_parent) {
  let snapshots;

  info(
    `Waiting for ${expected_content?.length ?? 0} content events and ` +
      `${expected_parent?.length ?? 0} parent events`
  );

  await TestUtils.waitForCondition(
    () => {
      snapshots = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      );

      return (
        (snapshots.parent?.length ?? 0) >= (expected_parent?.length ?? 0) &&
        (snapshots.content?.length ?? 0) >= (expected_content?.length ?? 0)
      );
    },
    "Wait for telemetry to be collected",
    100,
    100
  );

  info(JSON.stringify(snapshots, null, 2));

  if (expected_content !== undefined) {
    expected_content = expected_content.map(
      ([category, method, object, value, extra]) => {
        return { category, method, object, value, extra };
      }
    );

    let clear = expected_parent === undefined;

    TelemetryTestUtils.assertEvents(
      expected_content,
      {
        category: "creditcard",
      },
      { clear, process: "content" }
    );
  }

  if (expected_parent !== undefined) {
    expected_parent = expected_parent.map(
      ([category, method, object, value, extra]) => {
        return { category, method, object, value, extra };
      }
    );
    TelemetryTestUtils.assertEvents(
      expected_parent,
      {
        category: "creditcard",
      },
      { process: "parent" }
    );
  }
}

async function assertHistogram(histogramId, expectedNonZeroRanges) {
  let actualNonZeroRanges = {};
  await TestUtils.waitForCondition(
    () => {
      const snapshot = Services.telemetry
        .getHistogramById(histogramId)
        .snapshot();
      // Compute the actual ranges in the format { range1: value1, range2: value2 }.
      for (let [range, value] of Object.entries(snapshot.values)) {
        if (value > 0) {
          actualNonZeroRanges[range] = value;
        }
      }

      return (
        JSON.stringify(actualNonZeroRanges) ==
        JSON.stringify(expectedNonZeroRanges)
      );
    },
    "Wait for telemetry to be collected",
    100,
    100
  );

  Assert.equal(
    JSON.stringify(actualNonZeroRanges),
    JSON.stringify(expectedNonZeroRanges)
  );
}

async function openTabAndUseCreditCard(
  idx,
  creditCard,
  { closeTab = true, submitForm = true } = {}
) {
  let osKeyStoreLoginShown = null;

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    CREDITCARD_FORM_URL
  );
  if (OSKeyStore.canReauth()) {
    osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  }
  let browser = tab.linkedBrowser;

  await openPopupOn(browser, "form #cc-name");
  for (let i = 0; i <= idx; i++) {
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
  }
  await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
  if (osKeyStoreLoginShown) {
    await osKeyStoreLoginShown;
  }
  await waitForAutofill(browser, "#cc-number", creditCard["cc-number"]);
  await focusUpdateSubmitForm(
    browser,
    {
      focusSelector: "#cc-number",
      newValues: {},
    },
    submitForm
  );

  // flushing Glean data before tab removal (see Bug 1843178)
  await Services.fog.testFlushAllChildren();

  if (!closeTab) {
    return tab;
  }

  await BrowserTestUtils.removeTab(tab);
  return null;
}

add_setup(async function () {
  await clearGleanTelemetry();
});

add_task(async function test_popup_opened() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [ENABLED_AUTOFILL_CREDITCARDS_PREF, true],
      [AUTOFILL_CREDITCARDS_AVAILABLE_PREF, "on"],
    ],
  });

  Services.telemetry.clearEvents();
  await clearGleanTelemetry();

  await setStorage(TEST_CREDIT_CARD_1);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      await openPopupOn(browser, "#cc-number");
      await closePopup(browser);

      // flushing Glean data within withNewTab callback before tab removal (see Bug 1843178)
      await Services.fog.testFlushAllChildren();
    }
  );

  let expectedFormEvents = [
    ccFormArgsv2("detected", buildccFormv2Extra({ cc_exp: "false" }, "true")),
    ccFormArgsv2("popup_shown", { field_name: "cc-number" }),
  ];
  await assertTelemetry(undefined, expectedFormEvents);
  assertFormInteractionEventsInGlean(expectedFormEvents);
  assertDetectedCcNumberFieldsCountInGlean([
    { label: "cc_number_fields_1", count: 1 },
  ]);

  await removeAllRecords();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_popup_opened_form_without_autocomplete() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [ENABLED_AUTOFILL_CREDITCARDS_PREF, true],
      [AUTOFILL_CREDITCARDS_AVAILABLE_PREF, "on"],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "1",
      ],
    ],
  });

  Services.telemetry.clearEvents();
  await clearGleanTelemetry();

  await setStorage(TEST_CREDIT_CARD_1);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_WITHOUT_AUTOCOMPLETE_URL },
    async function (browser) {
      await openPopupOn(browser, "#cc-number");
      await closePopup(browser);

      // flushing Glean data within withNewTab callback before tab removal (see Bug 1843178)
      await Services.fog.testFlushAllChildren();
    }
  );

  let expectedFormEvents = [
    ccFormArgsv2(
      "detected",
      buildccFormv2Extra({ cc_number: "1", cc_name: "1", cc_exp: "false" }, "0")
    ),
    ccFormArgsv2("popup_shown", { field_name: "cc-number" }),
  ];
  await assertTelemetry(undefined, expectedFormEvents);
  assertFormInteractionEventsInGlean(expectedFormEvents);

  await removeAllRecords();
  await SpecialPowers.popPrefEnv();
});

add_task(
  async function test_popup_opened_form_without_autocomplete_separate_cc_number() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [ENABLED_AUTOFILL_CREDITCARDS_PREF, true],
        [AUTOFILL_CREDITCARDS_AVAILABLE_PREF, "on"],
        [
          "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
          "1",
        ],
      ],
    });

    Services.telemetry.clearEvents();
    await clearGleanTelemetry();

    await setStorage(TEST_CREDIT_CARD_1);

    // Click on the cc-number field of the form that only contains a cc-number field
    // (detected by Fathom)
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: CREDITCARD_FORM_WITHOUT_AUTOCOMPLETE_URL },
      async function (browser) {
        await openPopupOn(browser, "#form2-cc-number #cc-number");
        await closePopup(browser);

        // flushing Glean data within withNewTab callback before tab removal (see Bug 1843178)
        await Services.fog.testFlushAllChildren();
      }
    );

    let expectedFormEvents = [
      ccFormArgsv2("detected", buildccFormv2Extra({ cc_number: "1" }, "false")),
      ccFormArgsv2("popup_shown", { field_name: "cc-number" }),
    ];
    await assertTelemetry(undefined, expectedFormEvents);
    assertFormInteractionEventsInGlean(expectedFormEvents);

    await clearGleanTelemetry();

    // Then click on the cc-name field of the form that doesn't have a cc-number field
    // (detected by regexp-based heuristic)
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: CREDITCARD_FORM_WITHOUT_AUTOCOMPLETE_URL },
      async function (browser) {
        await openPopupOn(browser, "#form2-cc-other #cc-name");
        await closePopup(browser);

        // flushing Glean data within withNewTab callback before tab removal (see Bug 1843178)
        await Services.fog.testFlushAllChildren();
      }
    );

    expectedFormEvents = [
      ccFormArgsv2(
        "detected",
        buildccFormv2Extra(
          { cc_name: "1", cc_type: "0", cc_exp_month: "0", cc_exp_year: "0" },
          "false"
        )
      ),
      ccFormArgsv2("popup_shown", { field_name: "cc-name" }),
    ];
    await assertTelemetry(undefined, expectedFormEvents);

    assertFormInteractionEventsInGlean(expectedFormEvents);

    await removeAllRecords();
    await SpecialPowers.popPrefEnv();
  }
);

add_task(async function test_submit_creditCard_new() {
  async function test_per_command(
    command,
    idx,
    useCount = {},
    expectChanged = undefined
  ) {
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

    await assertHistogram(CC_NUM_USES_HISTOGRAM, useCount);

    await removeAllRecords();
    await SpecialPowers.popPrefEnv();
  }

  Services.telemetry.clearEvents();
  Services.telemetry.getHistogramById(CC_NUM_USES_HISTOGRAM).clear();
  await clearGleanTelemetry();

  let expectedFormInteractionEvents = [
    ccFormArgsv2("detected", buildccFormv2Extra({ cc_exp: "false" }, "true")),
    ccFormArgsv2(
      "submitted",
      buildccFormv2Extra({ cc_exp: "unavailable" }, "user_filled")
    ),
  ];

  await test_per_command(MAIN_BUTTON, undefined, { 1: 1 }, 1);

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

add_task(async function test_submit_creditCard_autofill() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  Services.telemetry.clearEvents();
  Services.telemetry.getHistogramById(CC_NUM_USES_HISTOGRAM).clear();
  await clearGleanTelemetry();

  await SpecialPowers.pushPrefEnv({
    set: [[ENABLED_AUTOFILL_CREDITCARDS_PREF, true]],
  });

  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  Assert.equal(creditCards.length, 1, "1 credit card in storage");

  await openTabAndUseCreditCard(0, TEST_CREDIT_CARD_1);

  await assertHistogram(CC_NUM_USES_HISTOGRAM, {
    1: 1,
  });

  SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);

  let expectedFormEvents = [
    ccFormArgsv2("detected", buildccFormv2Extra({ cc_exp: "false" }, "true")),
    ccFormArgsv2("popup_shown", { field_name: "cc-name" }),
    ccFormArgsv2(
      "filled",
      buildccFormv2Extra({ cc_exp: "unavailable" }, "filled")
    ),
    ccFormArgsv2(
      "submitted",
      buildccFormv2Extra({ cc_exp: "unavailable" }, "autofilled")
    ),
  ];
  await assertTelemetry(undefined, expectedFormEvents);

  assertFormInteractionEventsInGlean(expectedFormEvents);
  assertDetectedCcNumberFieldsCountInGlean([
    { label: "cc_number_fields_1", count: 1 },
  ]);

  await removeAllRecords();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_submit_creditCard_update() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  async function test_per_command(
    command,
    idx,
    useCount = {},
    expectChanged = undefined
  ) {
    await SpecialPowers.pushPrefEnv({
      set: [[ENABLED_AUTOFILL_CREDITCARDS_PREF, true]],
    });

    await setStorage(TEST_CREDIT_CARD_1);
    let creditCards = await getCreditCards();
    Assert.equal(creditCards.length, 1, "1 credit card in storage");

    let osKeyStoreLoginShown = null;
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: CREDITCARD_FORM_URL },
      async function (browser) {
        if (OSKeyStore.canReauth()) {
          osKeyStoreLoginShown =
            OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
        }
        let onPopupShown = waitForPopupShown();
        let onChanged;
        if (expectChanged !== undefined) {
          onChanged = TestUtils.topicObserved("formautofill-storage-changed");
        }

        await openPopupOn(browser, "form #cc-name");
        await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
        await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
        if (osKeyStoreLoginShown) {
          await osKeyStoreLoginShown;
        }

        await waitForAutofill(browser, "#cc-name", "John Doe");
        await focusUpdateSubmitForm(browser, {
          focusSelector: "#cc-name",
          newValues: {
            "#cc-exp-year": "2019",
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
            `There should be ${expectChanged} profile(s) stored.`
          );
        }
        // flushing Glean data within withNewTab callback before tab removal (see Bug 1843178)
        await Services.fog.testFlushAllChildren();
      }
    );

    await assertHistogram("CREDITCARD_NUM_USES", useCount);

    SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);

    await removeAllRecords();
  }

  Services.telemetry.clearEvents();
  Services.telemetry.getHistogramById(CC_NUM_USES_HISTOGRAM).clear();
  await clearGleanTelemetry();

  const expectedFormInteractionEvents = [
    ccFormArgsv2("detected", buildccFormv2Extra({ cc_exp: "false" }, "true")),
    ccFormArgsv2("popup_shown", { field_name: "cc-name" }),
    ccFormArgsv2(
      "filled",
      buildccFormv2Extra({ cc_exp: "unavailable" }, "filled")
    ),
    ccFormArgsv2("filled_modified", { field_name: "cc-exp-year" }),
    ccFormArgsv2(
      "submitted",
      buildccFormv2Extra(
        { cc_exp: "unavailable", cc_exp_year: "user_filled" },
        "autofilled"
      )
    ),
  ];

  await clearGleanTelemetry();

  await test_per_command(MAIN_BUTTON, undefined, { 1: 1 }, 1);

  await assertTelemetry(undefined, [
    ...expectedFormInteractionEvents,
    ["creditcard", "show", "update_doorhanger"],
    ["creditcard", "update", "update_doorhanger"],
  ]);

  assertFormInteractionEventsInGlean(expectedFormInteractionEvents);
  assertDetectedCcNumberFieldsCountInGlean([
    { label: "cc_number_fields_1", count: 1 },
  ]);

  await clearGleanTelemetry();

  await test_per_command(SECONDARY_BUTTON, undefined, { 0: 1, 1: 1 }, 2);

  await assertTelemetry(undefined, [
    ...expectedFormInteractionEvents,
    ["creditcard", "show", "update_doorhanger"],
    ["creditcard", "save", "update_doorhanger"],
  ]);

  assertFormInteractionEventsInGlean(expectedFormInteractionEvents);
  assertDetectedCcNumberFieldsCountInGlean([
    { label: "cc_number_fields_1", count: 1 },
  ]);
});

const TEST_SELECTORS = {
  selRecords: "#credit-cards",
  btnRemove: "#remove",
  btnAdd: "#add",
  btnEdit: "#edit",
};

const DIALOG_SIZE = "width=600,height=400";

add_task(async function test_removingCreditCardsViaKeyboardDelete() {
  Services.telemetry.clearEvents();

  await SpecialPowers.pushPrefEnv({
    set: [[ENABLED_AUTOFILL_CREDITCARDS_PREF, true]],
  });

  await setStorage(TEST_CREDIT_CARD_1);
  let win = window.openDialog(
    MANAGE_CREDIT_CARDS_DIALOG_URL,
    null,
    DIALOG_SIZE
  );
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);

  Assert.equal(selRecords.length, 1, "One credit card");

  EventUtils.synthesizeMouseAtCenter(selRecords.children[0], {}, win);
  EventUtils.synthesizeKey("VK_DELETE", {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsRemoved");
  Assert.equal(selRecords.length, 0, "No credit cards left");

  win.close();

  await assertTelemetry(undefined, [
    ["creditcard", "show", "manage"],
    ["creditcard", "delete", "manage"],
  ]);

  await removeAllRecords();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_saveCreditCard() {
  Services.telemetry.clearEvents();

  await SpecialPowers.pushPrefEnv({
    set: [[ENABLED_AUTOFILL_CREDITCARDS_PREF, true]],
  });

  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, win => {
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-number"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(
      "0" + TEST_CREDIT_CARD_1["cc-exp-month"].toString(),
      {},
      win
    );
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(
      TEST_CREDIT_CARD_1["cc-exp-year"].toString(),
      {},
      win
    );
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-name"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    info("saving credit card");
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
  });

  await assertTelemetry(undefined, [["creditcard", "add", "manage"]]);

  await removeAllRecords();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_editCreditCard() {
  Services.telemetry.clearEvents();

  await SpecialPowers.pushPrefEnv({
    set: [[ENABLED_AUTOFILL_CREDITCARDS_PREF, true]],
  });

  await setStorage(TEST_CREDIT_CARD_1);

  let creditCards = await getCreditCards();
  Assert.equal(creditCards.length, 1, "only one credit card is in storage");
  await testDialog(
    EDIT_CREDIT_CARD_DIALOG_URL,
    win => {
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_RIGHT", {}, win);
      EventUtils.synthesizeKey("test", {}, win);
      win.document.querySelector("#save").click();
    },
    {
      record: creditCards[0],
    }
  );

  await assertTelemetry(undefined, [
    ["creditcard", "show_entry", "manage"],
    ["creditcard", "edit", "manage"],
  ]);

  await removeAllRecords();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_histogram() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [[ENABLED_AUTOFILL_CREDITCARDS_PREF, true]],
  });

  Services.telemetry.getHistogramById(CC_NUM_USES_HISTOGRAM).clear();

  await setStorage(
    TEST_CREDIT_CARD_1,
    TEST_CREDIT_CARD_2,
    TEST_CREDIT_CARD_3,
    TEST_CREDIT_CARD_5
  );
  let creditCards = await getCreditCards();
  Assert.equal(creditCards.length, 4, "4 credit cards in storage");

  await assertHistogram(CC_NUM_USES_HISTOGRAM, {
    0: 4,
  });

  await openTabAndUseCreditCard(0, TEST_CREDIT_CARD_1);
  await assertHistogram(CC_NUM_USES_HISTOGRAM, {
    0: 3,
    1: 1,
  });

  await openTabAndUseCreditCard(1, TEST_CREDIT_CARD_2);
  await assertHistogram(CC_NUM_USES_HISTOGRAM, {
    0: 2,
    1: 2,
  });

  await openTabAndUseCreditCard(0, TEST_CREDIT_CARD_2);
  await assertHistogram(CC_NUM_USES_HISTOGRAM, {
    0: 2,
    1: 1,
    2: 1,
  });

  await openTabAndUseCreditCard(1, TEST_CREDIT_CARD_1);
  await assertHistogram(CC_NUM_USES_HISTOGRAM, {
    0: 2,
    2: 2,
  });

  await openTabAndUseCreditCard(2, TEST_CREDIT_CARD_5);
  await assertHistogram(CC_NUM_USES_HISTOGRAM, {
    0: 1,
    1: 1,
    2: 2,
  });

  await removeAllRecords();
  await SpecialPowers.popPrefEnv();

  await assertHistogram(CC_NUM_USES_HISTOGRAM, {});
});

add_task(async function test_clear_creditCard_autofill() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  Services.telemetry.clearEvents();
  Services.telemetry.getHistogramById(CC_NUM_USES_HISTOGRAM).clear();
  await clearGleanTelemetry();

  await SpecialPowers.pushPrefEnv({
    set: [[ENABLED_AUTOFILL_CREDITCARDS_PREF, true]],
  });

  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  Assert.equal(creditCards.length, 1, "1 credit card in storage");

  let tab = await openTabAndUseCreditCard(0, TEST_CREDIT_CARD_1, {
    closeTab: false,
    submitForm: false,
  });

  let expectedFormEvents = [
    ccFormArgsv2("detected", buildccFormv2Extra({ cc_exp: "false" }, "true")),
    ccFormArgsv2("popup_shown", { field_name: "cc-name" }),
    ccFormArgsv2(
      "filled",
      buildccFormv2Extra({ cc_exp: "unavailable" }, "filled")
    ),
  ];
  await assertTelemetry(undefined, expectedFormEvents);

  assertFormInteractionEventsInGlean(expectedFormEvents);
  assertDetectedCcNumberFieldsCountInGlean([
    { label: "cc_number_fields_1", count: 1 },
  ]);

  Services.telemetry.clearEvents();
  await clearGleanTelemetry();

  let browser = tab.linkedBrowser;

  let popupShown = BrowserTestUtils.waitForPopupEvent(
    browser.autoCompletePopup,
    "shown"
  );
  // Already focus in "cc-number" field, press 'down' to bring to popup.
  await BrowserTestUtils.synthesizeKey("KEY_ArrowDown", {}, browser);

  await popupShown;

  // flushing Glean data before tab removal (see Bug 1843178)
  await Services.fog.testFlushAllChildren();

  expectedFormEvents = [
    ccFormArgsv2("popup_shown", { field_name: "cc-number" }),
  ];
  await assertTelemetry(undefined, expectedFormEvents);
  assertFormInteractionEventsInGlean(expectedFormEvents);

  Services.telemetry.clearEvents();
  await clearGleanTelemetry();

  let popupHidden = BrowserTestUtils.waitForPopupEvent(
    browser.autoCompletePopup,
    "hidden"
  );

  // kPress Clear Form.
  await BrowserTestUtils.synthesizeKey("KEY_ArrowDown", {}, browser);
  await BrowserTestUtils.synthesizeKey("KEY_Enter", {}, browser);

  await popupHidden;

  popupShown = BrowserTestUtils.waitForPopupEvent(
    browser.autoCompletePopup,
    "shown"
  );

  await popupShown;

  // flushing Glean data before tab removal (see Bug 1843178)
  await Services.fog.testFlushAllChildren();

  expectedFormEvents = [
    ccFormArgsv2("cleared", { field_name: "cc-number" }),
    // popup is shown again because when the field is cleared and is focused,
    // we automatically triggers the popup.
    ccFormArgsv2("popup_shown", { field_name: "cc-number" }),
  ];

  await assertTelemetry(undefined, expectedFormEvents);

  assertFormInteractionEventsInGlean(expectedFormEvents);

  Services.telemetry.clearEvents();
  await clearGleanTelemetry();

  await BrowserTestUtils.removeTab(tab);

  await removeAllRecords();
  await SpecialPowers.popPrefEnv();
});
