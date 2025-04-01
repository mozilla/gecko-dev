"use strict";

add_task(async function test_fill_creditCard_with_failed_decryption() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  Services.telemetry.clearEvents();

  await setStorage(TEST_CREDIT_CARD_2);

  // We run setup on the OSKeyStore again. This will make decryption fail for
  // the card we just added because it was added in the old keystore.
  await OSKeyStoreTestUtils.cleanup();
  OSKeyStoreTestUtils.setup();

  // The OS keystore unlock should succeed
  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      await openPopupOn(browser, "#cc-name");

      const ccItem = getDisplayedPopupItems(browser)[0];
      let popupClosePromise = BrowserTestUtils.waitForPopupEvent(
        browser.autoCompletePopup,
        "hidden"
      );
      const autofillComplete = TestUtils.topicObserved(
        "formautofill-autofill-complete"
      );

      // Click on a credit card to attempt an autofill.
      await EventUtils.synthesizeMouseAtCenter(ccItem, {});
      await Promise.all([
        osKeyStoreLoginShown,
        popupClosePromise,
        autofillComplete,
      ]);

      // Nothing should have been autofilled.
      await SpecialPowers.spawn(browser, [], async function () {
        is(content.document.querySelector("#cc-name").value, "", "Check name");
        is(
          content.document.querySelector("#cc-number").value,
          "",
          "Check number"
        );
      });
    }
  );

  // Telemetry should have registered a decryption error.
  await Services.fog.testFlushAllChildren();
  let testEvents = Glean.creditcard.osKeystoreDecrypt.testGetValue();
  is(testEvents.length, 1, "Event was recorded");
  is(testEvents[0].extra.trigger, "autofill", "Trigger was correct");
  is(
    testEvents[0].extra.isDecryptSuccess,
    "false",
    "Decryption was recorded as failed"
  );
  is(
    testEvents[0].extra.errorResult,
    Cr.NS_ERROR_FAILURE.toString(),
    "Result was abort"
  );
});
