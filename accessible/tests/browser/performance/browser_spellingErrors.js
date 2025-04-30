/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

addAccessibleTask(
  `<div id="editable" contenteditable></div>`,
  async function testRemoveManySpellingErrors(browser) {
    let changed = waitForEvent(EVENT_TEXT_ATTRIBUTE_CHANGED);
    await invokeContentTask(browser, [], () => {
      const editable = content.document.getElementById("editable");
      for (let i = 0; i < 500; ++i) {
        const p = content.document.createElement("p");
        p.textContent = "tset tset tset";
        editable.append(p);
      }
      editable.focus();
    });
    // Spell checking is async, so we need to wait for it to happen.
    info("Waiting for spell check");
    await changed;
    await timeThis("removeManySpellingErrors", async () => {
      info("Removing all content from editable");
      let reorder = waitForEvent(EVENT_REORDER, "editable");
      await invokeContentTask(browser, [], () => {
        content.document.getElementById("editable").innerHTML = "";
      });
      info("Waiting for reorder event");
      await reorder;
    });
  }
);
