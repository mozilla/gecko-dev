"use strict";

add_task(async function test_fill_creditCard_with_failed_decryption() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  // This was copied from another test but according to data folks
  // it is optional.
  // await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  Services.telemetry.clearEvents();

  await setStorage(TEST_CREDIT_CARD_2);

  // We run setup on the OSKeyStore again. This will make decryption fail for
  // the card we just added because it was added in the old keystore.
  info("Resetting the OSKeyStore");
  await OSKeyStoreTestUtils.cleanup();
  OSKeyStoreTestUtils.setup();

  // We do not support reauth on Linux: Bug 1527745
  info("Setting up OS auth test promise (Win & Mac only)");
  let osKeyStoreLoginShown = Promise.resolve();
  if (OSKeyStore.canReauth()) {
    // The OS keystore unlock should succeed.
    osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  }

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      info("Waiting for CC popup");
      await openPopupOn(browser, "#cc-name");

      const ccItem = getDisplayedPopupItems(browser)[0];
      let popupClosePromise = BrowserTestUtils.waitForPopupEvent(
        browser.autoCompletePopup,
        "hidden"
      );
      const autofillComplete = TestUtils.topicObserved(
        "formautofill-autofill-complete"
      );

      info("Synthesizing click on credit card");
      await EventUtils.synthesizeMouseAtCenter(ccItem, {});
      info("Awaiting three events");
      await Promise.all([
        osKeyStoreLoginShown.then(() =>
          info("osKeyStoreLoginShown successful")
        ),
        popupClosePromise.then(() => info("popupClosePromise successful")),
        autofillComplete.then(() => info("autofillComplete successful")),
      ]);

      info("Checking that the CC was not auto-filled");
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
