/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test exposure of group position information.
 */
addAccessibleTask(
  `
<ul>
  <li id="li1">li1<ul>
    <li id="li1a">li1a</li>
  </ul></li>
  <li id="li2">li2</li>
</ul>
<h2 id="h2">h2</h2>
<button id="button">button</button>
  `,
  async function testGroupPosition() {
    let pos = await runPython(`
      global doc, li1
      doc = getDocIa2()
      li1 = findIa2ByDomId(doc, "li1")
      return li1.groupPosition
    `);
    SimpleTest.isDeeply(pos, [1, 2, 1], "li1 groupPosition correct");
    let attrs = await runPython(`li1.attributes`);
    ok(!attrs.includes("posinset:"), "li1 doesn't have posinset attribute");
    ok(!attrs.includes("setsize:"), "li1 doesn't have setsize attribute");
    ok(!attrs.includes("level:"), "li1 doesn't have level attribute");

    pos = await runPython(`
      global li1a
      li1a = findIa2ByDomId(doc, "li1a")
      return li1a.groupPosition
    `);
    SimpleTest.isDeeply(pos, [2, 1, 1], "li1a groupPosition correct");
    attrs = await runPython(`li1a.attributes`);
    ok(!attrs.includes("posinset:"), "li1a doesn't have posinset attribute");
    ok(!attrs.includes("setsize:"), "li1a doesn't have setsize attribute");
    ok(!attrs.includes("level:"), "li1a doesn't have level attribute");

    pos = await runPython(`
      global li2
      li2 = findIa2ByDomId(doc, "li2")
      return li2.groupPosition
    `);
    SimpleTest.isDeeply(pos, [1, 2, 2], "li2 groupPosition correct");
    attrs = await runPython(`li2.attributes`);
    ok(!attrs.includes("posinset:"), "li2 doesn't have posinset attribute");
    ok(!attrs.includes("setsize:"), "li2 doesn't have setsize attribute");
    ok(!attrs.includes("level:"), "li2 doesn't have level attribute");

    pos = await runPython(`
      global h2
      h2 = findIa2ByDomId(doc, "h2")
      return h2.groupPosition
    `);
    // IAccessible2 expects heading level to be exposed as an object attribute,
    // not via groupPosition.
    SimpleTest.isDeeply(pos, [0, 0, 0], "h2 groupPosition correct");
    attrs = await runPython(`h2.attributes`);
    ok(!attrs.includes("posinset:"), "h2 doesn't have posinset attribute");
    ok(!attrs.includes("setsize:"), "h2 doesn't have setsize attribute");
    ok(attrs.includes("level:2;"), "h2 has level attribute 2");

    pos = await runPython(`
      global button
      button = findIa2ByDomId(doc, "button")
      return button.groupPosition
    `);
    SimpleTest.isDeeply(pos, [0, 0, 0], "button groupPosition correct");
    attrs = await runPython(`button.attributes`);
    ok(!attrs.includes("posinset:"), "button doesn't have posinset attribute");
    ok(!attrs.includes("setsize:"), "button doesn't have setsize attribute");
    ok(!attrs.includes("level:"), "button doesn't have level attribute");
  }
);
