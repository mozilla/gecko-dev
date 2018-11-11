/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

add_task(async function setup() {
  let {formAutofillStorage} = ChromeUtils.import("resource://formautofill/FormAutofillStorage.jsm", {});
  await formAutofillStorage.initialize();
});

add_task(async function test_cancelEditCreditCardDialog() {
  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, (win) => {
    win.document.querySelector("#cancel").click();
  });
});

add_task(async function test_cancelEditCreditCardDialogWithESC() {
  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, (win) => {
    EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  });
});

add_task(async function test_saveCreditCard() {
  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, (win) => {
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-number"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("0" + TEST_CREDIT_CARD_1["cc-exp-month"].toString(), {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-exp-year"].toString(), {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-name"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-type"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    info("saving credit card");
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
  });
  let creditCards = await getCreditCards();

  is(creditCards.length, 1, "only one credit card is in storage");
  for (let [fieldName, fieldValue] of Object.entries(TEST_CREDIT_CARD_1)) {
    if (fieldName === "cc-number") {
      fieldValue = "*".repeat(fieldValue.length - 4) + fieldValue.substr(-4);
    }
    is(creditCards[0][fieldName], fieldValue, "check " + fieldName);
  }
  is(creditCards[0].billingAddressGUID, undefined, "check billingAddressGUID");
  ok(creditCards[0]["cc-number-encrypted"], "cc-number-encrypted exists");
});

add_task(async function test_saveCreditCardWithMaxYear() {
  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, (win) => {
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_2["cc-number"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_2["cc-exp-month"].toString(), {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_2["cc-exp-year"].toString(), {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_2["cc-name"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-type"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    info("saving credit card");
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
  });
  let creditCards = await getCreditCards();

  is(creditCards.length, 2, "Two credit cards are in storage");
  for (let [fieldName, fieldValue] of Object.entries(TEST_CREDIT_CARD_2)) {
    if (fieldName === "cc-number") {
      fieldValue = "*".repeat(fieldValue.length - 4) + fieldValue.substr(-4);
    }
    is(creditCards[1][fieldName], fieldValue, "check " + fieldName);
  }
  ok(creditCards[1]["cc-number-encrypted"], "cc-number-encrypted exists");
  await removeCreditCards([creditCards[1].guid]);
});

add_task(async function test_saveCreditCardWithBillingAddress() {
  await saveAddress(TEST_ADDRESS_4);
  await saveAddress(TEST_ADDRESS_1);
  let addresses = await getAddresses();
  let billingAddress = addresses[0];

  const TEST_CREDIT_CARD = Object.assign({}, TEST_CREDIT_CARD_2, {
    billingAddressGUID: billingAddress.guid,
  });

  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, (win) => {
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD["cc-number"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD["cc-exp-month"].toString(), {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD["cc-exp-year"].toString(), {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD["cc-name"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD["cc-type"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(billingAddress["given-name"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    info("saving credit card");
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
  });
  let creditCards = await getCreditCards();

  is(creditCards.length, 2, "Two credit cards are in storage");
  for (let [fieldName, fieldValue] of Object.entries(TEST_CREDIT_CARD)) {
    if (fieldName === "cc-number") {
      fieldValue = "*".repeat(fieldValue.length - 4) + fieldValue.substr(-4);
    }
    is(creditCards[1][fieldName], fieldValue, "check " + fieldName);
  }
  ok(creditCards[1].billingAddressGUID, "billingAddressGUID is truthy");
  ok(creditCards[1]["cc-number-encrypted"], "cc-number-encrypted exists");
  await removeCreditCards([creditCards[1].guid]);
  await removeAddresses([
    addresses[0].guid,
    addresses[1].guid,
  ]);
});

add_task(async function test_editCreditCard() {
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "only one credit card is in storage");
  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, (win) => {
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_RIGHT", {}, win);
    EventUtils.synthesizeKey("test", {}, win);
    win.document.querySelector("#save").click();
  }, {
    record: creditCards[0],
  });
  ok(true, "Edit credit card dialog is closed");
  creditCards = await getCreditCards();

  is(creditCards.length, 1, "only one credit card is in storage");
  is(creditCards[0]["cc-name"], TEST_CREDIT_CARD_1["cc-name"] + "test", "cc name changed");
  await removeCreditCards([creditCards[0].guid]);

  creditCards = await getCreditCards();
  is(creditCards.length, 0, "Credit card storage is empty");
});

add_task(async function test_editCreditCardWithMissingBillingAddress() {
  const TEST_CREDIT_CARD = Object.assign({}, TEST_CREDIT_CARD_2, {
    billingAddressGUID: "unknown-guid",
  });
  await saveCreditCard(TEST_CREDIT_CARD);

  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "one credit card in storage");
  is(creditCards[0].billingAddressGUID, TEST_CREDIT_CARD.billingAddressGUID,
     "Check saved billingAddressGUID");
  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, (win) => {
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_RIGHT", {}, win);
    EventUtils.synthesizeKey("test", {}, win);
    win.document.querySelector("#save").click();
  }, {
    record: creditCards[0],
  });
  ok(true, "Edit credit card dialog is closed");
  creditCards = await getCreditCards();

  is(creditCards.length, 1, "only one credit card is in storage");
  is(creditCards[0]["cc-name"], TEST_CREDIT_CARD["cc-name"] + "test", "cc name changed");
  is(creditCards[0].billingAddressGUID, undefined,
     "unknown GUID removed upon manual save");
  await removeCreditCards([creditCards[0].guid]);

  creditCards = await getCreditCards();
  is(creditCards.length, 0, "Credit card storage is empty");
});

add_task(async function test_addInvalidCreditCard() {
  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, (win) => {
    const unloadHandler = () => ok(false, "Edit credit card dialog shouldn't be closed");
    win.addEventListener("unload", unloadHandler);

    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("test", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("test name", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeMouseAtCenter(win.document.querySelector("#save"), {}, win);

    is(win.document.querySelector("form").checkValidity(), false, "cc-number is invalid");
    SimpleTest.requestFlakyTimeout("Ensure the window remains open after save attempt");
    setTimeout(() => {
      win.removeEventListener("unload", unloadHandler);
      info("closing");
      win.close();
    }, 500);
  });
  info("closed");
  let creditCards = await getCreditCards();

  is(creditCards.length, 0, "Credit card storage is empty");
});

add_task(async function test_editCardWithInvalidNetwork() {
  const TEST_CREDIT_CARD = Object.assign({}, TEST_CREDIT_CARD_2, {
    "cc-type": "asiv",
  });
  await saveCreditCard(TEST_CREDIT_CARD);

  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "one credit card in storage");
  is(creditCards[0]["cc-type"], TEST_CREDIT_CARD["cc-type"],
     "Check saved cc-type");
  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, (win) => {
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_RIGHT", {}, win);
    EventUtils.synthesizeKey("test", {}, win);
    win.document.querySelector("#save").click();
  }, {
    record: creditCards[0],
  });
  ok(true, "Edit credit card dialog is closed");
  creditCards = await getCreditCards();

  is(creditCards.length, 1, "only one credit card is in storage");
  is(creditCards[0]["cc-name"], TEST_CREDIT_CARD["cc-name"] + "test", "cc name changed");
  is(creditCards[0]["cc-type"], undefined,
     "unknown cc-type removed upon manual save");
  await removeCreditCards([creditCards[0].guid]);

  creditCards = await getCreditCards();
  is(creditCards.length, 0, "Credit card storage is empty");
});
