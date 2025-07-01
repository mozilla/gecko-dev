/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gInvalidFormPopup =
  gBrowser.selectedBrowser.browsingContext.currentWindowGlobal
    .getActor("FormValidation")
    ._getAndMaybeCreatePanel(document);

function getValidationMessage() {
  return gInvalidFormPopup.textContent ?? "";
}

/**
 * Test that the form validation popup updates its message when the
 * validation message changes while the popup is shown.
 */
add_task(async function test_validation_popup_message_update() {
  ok(
    gInvalidFormPopup,
    "The browser should have a popup to show when a form is invalid"
  );
  await BrowserTestUtils.withNewTab(
    "https://example.com/nothere",
    async function checkTab(browser) {
      // Inject a form with an input and set an initial invalid state
      let popupShown = BrowserTestUtils.waitForPopupEvent(
        gInvalidFormPopup,
        "shown"
      );
      await SpecialPowers.spawn(browser, [], () => {
        let doc = content.document;
        let form = doc.createElement("form");
        let input = doc.createElement("input");
        input.required = true;
        form.append(input);
        doc.body.append(form);
        // Set initial custom validity
        input.setCustomValidity("First error message");
        content.eval(`document.querySelector('input').reportValidity();`);
      });
      await popupShown;
      Assert.stringContains(
        getValidationMessage(),
        "First error message",
        "Popup should show the first error message"
      );

      // Change the message and trigger validation again
      let popupUpdated = BrowserTestUtils.waitForMutationCondition(
        gInvalidFormPopup,
        { subtree: true, childList: true },
        () => getValidationMessage().includes("Second error message")
      );
      await SpecialPowers.spawn(browser, [], () => {
        let input = content.document.querySelector("input");
        input.setCustomValidity("Second error message");
        content.eval(`document.querySelector('input').reportValidity();`);
      });
      info("Waiting for popup to update with new message");
      await popupUpdated;
      Assert.stringContains(
        getValidationMessage(),
        "Second error message",
        "Popup should update to show the new error message"
      );
      let popupHidden = BrowserTestUtils.waitForPopupEvent(
        gInvalidFormPopup,
        "hidden"
      );
      gInvalidFormPopup.hidePopup();
      await popupHidden;
    }
  );
});
