/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function testComboBox(browser, accDoc, suppressPopupInValueTodo = false) {
  const box = getNativeInterface(accDoc, "box");
  is(box.getAttributeValue("AXRole"), "AXComboBox");
  is(box.getAttributeValue("AXValue"), "peach", "Initial value correct");

  let expandedChanged = waitForMacEvent("AXExpandedChanged", "box");
  let didBoxValueChange = false;
  waitForMacEvent("AXValueChanged", "box").then(() => {
    didBoxValueChange = true;
  });
  await invokeContentTask(browser, [], () => {
    const b = content.document.getElementById("box");
    b.ariaExpanded = true;
  });

  await expandedChanged;
  if (suppressPopupInValueTodo) {
    todo(
      !didBoxValueChange,
      "Value of combobox did not change when it was opened"
    );
    todo_is(box.getAttributeValue("AXValue"), "peach");
  } else {
    ok(
      !didBoxValueChange,
      "Value of combobox did not change when it was opened"
    );
    is(box.getAttributeValue("AXValue"), "peach", "After popup value correct");
  }
}

addAccessibleTask(
  `
  <style>
    #box[aria-expanded=false] > ul {
        visibility: hidden;
    }
  </style>
  <div role="combobox" id="box" aria-expanded="false" aria-haspopup="listbox">
    <input id="input" value="peach" aria-autocomplete="list" aria-controls="controlled_listbox">
    <ul role="listbox" id="controlled_listbox">
      <li role="option">apple</li>
      <li role="option">peach</li>
    </ul>
  </div>`,
  async (browser, accDoc) => {
    info("Test ARIA 1.1 style combobox (role on container of entry and list)");
    await testComboBox(browser, accDoc);
  }
);

addAccessibleTask(
  `
  <style>
   #box[aria-expanded=false] + ul {
       visibility: hidden;
   }
  </style>
  <input type="text" id="box" role="combobox" value="peach"
         aria-owns="owned_listbox"
         aria-expanded="false"
         aria-haspopup="listbox"
         aria-autocomplete="list" >
  <ul role="listbox" id="owned_listbox">
    <li role="option">apple</li>
    <li role="option">peach</li>
  </ul>
`,
  async (browser, accDoc) => {
    info("Test ARIA 1.0 style combobox (entry aria-owns list)");
    // XXX: Bug 1912520
    await testComboBox(browser, accDoc, true);
  }
);

addAccessibleTask(
  `
  <style>
   #box[aria-expanded=false] + ul {
     visibility: hidden;
   }
  </style>
  <input type="text" id="box" role="combobox" value="peach"
         aria-controls="controlled_listbox"
         aria-expanded="false"
         aria-haspopup="listbox"
         aria-autocomplete="list" >
  <ul role="listbox" id="controlled_listbox">
    <li role="option">apple</li>
    <li role="option">peach</li>
  </ul>
`,
  async (browser, accDoc) => {
    info("Test ARIA 1.2 style combobox (entry aria-controls list)");
    await testComboBox(browser, accDoc);
  }
);
