/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Ensure explicit selection gets priority over implicit selection
addAccessibleTask(
  `
  <div role="listbox" id="listbox">
    <div role="option" aria-selected="true" id="o1">a</div>
    <div role="option" tabindex="0" id="o2">b</div>
  </div>
  `,
  async function testExplicitSelection(browser, accDoc) {
    const o1 = findAccessibleChildByID(accDoc, "o1");
    const o2 = findAccessibleChildByID(accDoc, "o2");

    await untilCacheOk(() => {
      const [states] = getStates(o1);
      return (states & STATE_SELECTED) != 0;
    }, "option 1 should be selected");
    await untilCacheOk(() => {
      const [states] = getStates(o2);
      return (states & STATE_SELECTED) == 0;
    }, "option 2 should NOT be selected");

    // Focus the second option.
    const e = waitForEvents({
      expected: [[EVENT_FOCUS, "o2"]],
      unexpected: [[EVENT_SELECTION, "o2"]],
    });
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("o2").focus();
    });
    await e;

    await untilCacheOk(() => {
      const [states] = getStates(o1);
      return (states & STATE_SELECTED) != 0;
    }, "option 1 should be selected");
    await untilCacheOk(() => {
      const [states] = getStates(o2);
      return (states & STATE_SELECTED) == 0 && (states & STATE_FOCUSED) != 0;
    }, "option 2 should NOT be selected but should be focused");
  },
  { chrome: true, iframe: true, remoteIframe: true }
);
