/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

addAccessibleTask(
  `<div id="container" role="group"></div>`,
  async function testManyShowEvents(browser) {
    await timeThis("manyShowEvents", async () => {
      info("Adding many children");
      let lastShown = waitForEvent(EVENT_SHOW, "last");
      await invokeContentTask(browser, [], () => {
        const container = content.document.getElementById("container");
        for (let c = 0; c < 100000; ++c) {
          const child = content.document.createElement("p");
          child.textContent = c;
          container.append(child);
        }
        container.lastChild.id = "last";
      });
      info("Waiting for last show event");
      await lastShown;
    });
  }
);
