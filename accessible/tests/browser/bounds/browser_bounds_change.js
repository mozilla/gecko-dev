/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Layout } = ChromeUtils.importESModule(
  "chrome://mochitests/content/browser/accessible/tests/browser/Layout.sys.mjs"
);

/**
 * Test adding children to body that recieves its own accessible.
 */
addAccessibleTask(
  `
  <style>

  body {
    overflow: hidden;
  }

  button {
    display: block;
    height: 2em;
    width: 100%;
  }
</style>
`,
  async function (browser, docAcc) {
    const r = waitForEvent(EVENT_REORDER);
    await invokeContentTask(browser, [], () => {
      content.requestAnimationFrame(() => {
        let btn = content.document.createElement("button");
        btn.textContent = "hello";
        content.document.body.appendChild(btn);
      });
    });
    await r;

    const dpr = await getContentDPR(browser);
    const bodyAcc = docAcc.firstChild;
    is(
      getAccessibleDOMNodeID(bodyAcc),
      "body",
      "Doc's child is body container"
    );
    const bodyHeightNotZero = () => {
      let [, , , height] = Layout.getBounds(bodyAcc, dpr);
      info(`height: ${height}`);
      return height > 0;
    };

    await untilCacheOk(bodyHeightNotZero, "Body height is not 0");
  }
);
