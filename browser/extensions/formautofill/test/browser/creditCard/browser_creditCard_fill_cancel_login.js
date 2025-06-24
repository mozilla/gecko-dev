"use strict";

add_task(async function test_fill_creditCard_but_cancel_login() {
  if (!OSKeyStore.canReauth()) {
    info("Cannot test login cancel when there is no prompt.");
    return;
  }

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  Services.telemetry.clearEvents();

  await setStorage(TEST_CREDIT_CARD_2);

  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(false); // cancel
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      await openPopupOn(browser, "#cc-name");
      const ccItem = getDisplayedPopupItems(browser)[0];
      let popupClosePromise = BrowserTestUtils.waitForPopupEvent(
        browser.autoCompletePopup,
        "hidden"
      );
      await EventUtils.synthesizeMouseAtCenter(ccItem, {});
      await Promise.all([osKeyStoreLoginShown, popupClosePromise]);

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
    Cr.NS_ERROR_ABORT.toString(),
    "Result was abort"
  );
});
