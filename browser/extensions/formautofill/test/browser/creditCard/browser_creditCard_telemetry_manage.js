"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/extensions/formautofill/test/browser/creditCard/browser_telemetry_utils.js",
  this
);

const TEST_SELECTORS = {
  selRecords: "#credit-cards",
  btnRemove: "#remove",
  btnAdd: "#add",
  btnEdit: "#edit",
};

const DIALOG_SIZE = "width=600,height=400";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["test.wait300msAfterTabSwitch", true]],
  });
});

add_task(async function test_removingCreditCardsViaKeyboardDelete() {
  const cleanupFunc = await setupTask(
    {
      set: [[ENABLED_AUTOFILL_CREDITCARDS_PREF, true]],
    },
    TEST_CREDIT_CARD_1
  );

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

  await cleanupFunc();
});

add_task(async function test_saveCreditCard() {
  const cleanupFunc = await setupTask({
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

  await cleanupFunc();
});

add_task(async function test_editCreditCard() {
  await removeAllRecords();
  const cleanupFunc = await setupTask(
    {
      set: [[ENABLED_AUTOFILL_CREDITCARDS_PREF, true]],
    },
    TEST_CREDIT_CARD_1
  );

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

  await cleanupFunc();
});

add_task(async function test_histogram() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  const cleanupFunc = await setupTask(
    {
      set: [[ENABLED_AUTOFILL_CREDITCARDS_PREF, true]],
    },
    TEST_CREDIT_CARD_1,
    TEST_CREDIT_CARD_2,
    TEST_CREDIT_CARD_3,
    TEST_CREDIT_CARD_5
  );

  let creditCards = await getCreditCards();
  Assert.equal(creditCards.length, 4, "4 credit cards in storage");

  await openTabAndUseCreditCard(0, TEST_CREDIT_CARD_1);

  await openTabAndUseCreditCard(1, TEST_CREDIT_CARD_2);

  await openTabAndUseCreditCard(0, TEST_CREDIT_CARD_2);

  await openTabAndUseCreditCard(1, TEST_CREDIT_CARD_1);

  await openTabAndUseCreditCard(2, TEST_CREDIT_CARD_5);

  await cleanupFunc();
});
