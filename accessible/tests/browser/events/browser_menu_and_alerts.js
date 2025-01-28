/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function matchHideParent(parent) {
  return event => {
    event.QueryInterface(Ci.nsIAccessibleHideEvent);
    if (typeof parent == "string") {
      return getAccessibleDOMNodeID(event.targetParent) == parent;
    }

    return event.targetParent == parent;
  };
}

/**
 * Accessible menu popup start/end
 */
addAccessibleTask(
  `
  <div id="container">
  <ul id="menu" role="menu" style="display: none;">
    <li role="menuitem">Hello</li>
  </ul>
  </div>
  `,
  async function (browser) {
    let expectedEvents = waitForEvents(
      [
        [EVENT_SHOW, "menu"],
        [EVENT_REORDER, "container"],
        [EVENT_MENUPOPUP_START, "menu"],
      ],
      "Popup start sequence",
      true
    );
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("menu").style.display = "block";
    });
    await expectedEvents;

    expectedEvents = waitForEvents(
      [
        [EVENT_MENUPOPUP_END, "menu"],
        [EVENT_HIDE, matchHideParent("container")],
        [EVENT_REORDER, "container"],
      ],
      "Popup end sequence",
      true
    );
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("menu").style.display = "none";
    });
    await expectedEvents;
  },
  { chrome: true, topLevel: true }
);

/**
 * Accessible menu popup start/end in subtree
 */
addAccessibleTask(
  `
  <div id="container" style="display: none;">
  <ul id="menu" role="menu">
    <li role="menuitem">Hello</li>
  </ul>
  </div>
  `,
  async function (browser, accDoc) {
    let expectedEvents = waitForEvents(
      [
        [EVENT_SHOW, "container"],
        [EVENT_REORDER, accDoc],
        [EVENT_MENUPOPUP_START, "menu"],
      ],
      "Embedded popup start sequence",
      true
    );
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("container").style.display = "block";
    });
    await expectedEvents;

    expectedEvents = waitForEvents(
      [
        [EVENT_MENUPOPUP_END, "menu"],
        [EVENT_HIDE, matchHideParent(accDoc)],
        [EVENT_REORDER, accDoc],
      ],
      "Embedded popup end sequence",
      true
    );
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("container").style.display = "none";
    });
    await expectedEvents;
  },
  { chrome: true, topLevel: true }
);

/**
 * Accessible alert event
 */
addAccessibleTask(
  `
  <div id="container">
  <div role="alert" id="alert" style="display: none;">Alert!</div>
  </div>
  `,
  async function (browser, accDoc) {
    let expectedEvents = waitForEvents(
      [[EVENT_SHOW], [EVENT_REORDER, "container"], [EVENT_ALERT, "alert"]],
      "Alert events sequence",
      true
    );
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("alert").style.display = "block";
    });
    await expectedEvents;

    expectedEvents = waitForEvent(EVENT_REORDER, accDoc);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("container").style.display = "none";
    });
    await expectedEvents;

    expectedEvents = waitForEvents(
      [[EVENT_SHOW], [EVENT_REORDER, accDoc], [EVENT_ALERT, "alert"]],
      "Embedded Alert events sequence",
      true
    );
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("container").style.display = "block";
    });
    await expectedEvents;
  },
  { chrome: true, topLevel: true }
);

/**
 * No alert event at load.
 */
addAccessibleTask(
  ``,
  async function (browser) {
    let onLoadEvents = waitForEvents({
      expected: [
        [EVENT_FOCUS, "body2"],
        [EVENT_DOCUMENT_LOAD_COMPLETE, "body2"],
        stateChangeEventArgs("body2", STATE_BUSY, false, false),
      ],
      unexpected: [[EVENT_ALERT]],
    });

    BrowserTestUtils.startLoadingURIString(
      browser,
      `data:text/html;charset=utf-8,
      <html><body id="body2">
        <div role="alert" id="alert">Alert!</div>
      </body></html>`
    );

    await onLoadEvents;
  },
  { chrome: true, topLevel: true }
);
