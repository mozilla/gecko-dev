/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/states.js */
loadScripts({ name: "states.js", dir: MOCHITESTS_DIR });

/**
 * Test invalid state and aria-invalid attribute on a checkbox.
 */
addAccessibleTask(
  `
  <form>
    <input type="checkbox" required id="box"><label for="box">I am required
  </form>
   `,
  async (browser, accDoc) => {
    // Check initial AXInvalid values are correct
    let box = getNativeInterface(accDoc, "box");
    // XXX: bug 1967000
    await untilCacheOk(() => {
      return box.getAttributeValue("AXInvalid") == "true";
    }, "Correct invalid value for box");

    // Remove required attr, verify AXInvalid is updated
    let stateChanged = waitForEvent(EVENT_STATE_CHANGE, "box");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("box").removeAttribute("required");
    });
    await stateChanged;

    await untilCacheOk(() => {
      return box.getAttributeValue("AXInvalid") == "false";
    }, "Correct invalid value after required attr removed");

    // Remove add aria-invliad attr, verify AXInvalid is updated
    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "box");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("box")
        .setAttribute("aria-invalid", "true");
    });
    await stateChanged;

    await untilCacheOk(() => {
      return box.getAttributeValue("AXInvalid") == "true";
    }, "Correct invalid value after aria-invalid attr set to true");
  }
);
