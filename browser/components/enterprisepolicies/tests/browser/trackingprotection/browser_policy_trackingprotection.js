/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_enabletrackingprotection_strict() {
  await BrowserTestUtils.withNewTab(
    "about:preferences#privacy",
    async browser => {
      let strictRadio = browser.contentDocument.getElementById("strictRadio");
      is(strictRadio.selected, true, "Strict checkbox should be selected");
      is(strictRadio.disabled, true, "Strict checkbox should be disabled");
    }
  );
});
