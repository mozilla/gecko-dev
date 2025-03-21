/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
add_task(async function test() {
  info("Test the more actions button in about:profiling.");

  await withAboutProfiling(async (document, browser) => {
    info("Test that there is a button to show a menu with more actions.");
    const moreActionsButton = document.querySelector("moz-button");
    ok(moreActionsButton, "There is a button.");
    ok(moreActionsButton.shadowRoot, "The button contains a shadowDom.");

    // Make sure we have an accessible name
    is(
      moreActionsButton.shadowRoot.querySelector("button").title,
      "More actions",
      "Test that the more actions button has a title"
    );

    info("Test that the button is clickable");
    // The second argument is the event object. By passing an empty object, this
    // tells the utility function to generate a mousedown then a mouseup, that
    // is a click.
    await BrowserTestUtils.synthesizeMouseAtCenter("moz-button", {}, browser);
    const item = await getElementFromDocumentByText(
      document,
      "To be continued"
    );
    ok(item, "The item has been displayed");
  });
});
