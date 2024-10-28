/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test that EVENT_SYSTEM_SCROLLINGSTART fires on the hypertext parent for text
 * fragments.
 */
addAccessibleTask(
  `
<p id="first">The first phrase.</p>
<p id="second">The <i>second <b>phrase.</b></i></p>
  `,
  async function testTextFragment(browser) {
    info("Navigating to text fragment: second phrase");
    await runPython(`
      global scrolled
      scrolled = WaitForWinEvent(EVENT_SYSTEM_SCROLLINGSTART, "second")
    `);
    await invokeContentTask(browser, [], () => {
      content.location.hash = "#:~:text=second%20phrase";
    });
    await runPython(`
      scrolled.wait()
    `);
    ok(true, "second paragraph got EVENT_SYSTEM_SCROLLINGSTART");
  }
);
