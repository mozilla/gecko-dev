/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

addAccessibleTask(
  `
<p id="p">p</p>
<a href="https://example.com" id="link">a</a>
<input id="range_input" type="range" min="0" max="10" value="8">
<input id="text_input" type="text" value="hello">
<button id="button">hello</button>`,
  async function testInterfaces() {
    await runPython(`
      global doc
      doc = getDoc()
    `);

    async function checkInterfaces(id, expectedInterfaces) {
      let interfaces = await runPython(`
        return findByDomId(doc, "${id}").get_interfaces()
      `);

      Assert.deepEqual(
        expectedInterfaces.slice().sort(),
        interfaces.sort(),
        `Correct interfaces for "${id}"`
      );
    }

    await checkInterfaces("p", [
      "Accessible",
      "Collection",
      "Component",
      "EditableText",
      "Hyperlink",
      "Hypertext",
      "Text",
    ]);
    await checkInterfaces("link", [
      "Accessible",
      "Action",
      "Collection",
      "Component",
      "EditableText",
      "Hyperlink",
      "Hypertext",
      "Text",
    ]);
    await checkInterfaces("range_input", [
      "Accessible",
      "Collection",
      "Component",
      "Hyperlink",
      "Value",
    ]);
    await checkInterfaces("text_input", [
      "Accessible",
      "Action",
      "Collection",
      "Component",
      "EditableText",
      "Hyperlink",
      "Hypertext",
      "Text",
    ]);
    await checkInterfaces("button", [
      "Accessible",
      "Action",
      "Collection",
      "Component",
      "EditableText",
      "Hyperlink",
      "Hypertext",
      "Text",
    ]);
  },
  { chrome: true, topLevel: true }
);
