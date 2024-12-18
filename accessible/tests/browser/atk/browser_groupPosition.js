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
    let attrs = await runPython(`
      global doc
      doc = getDoc()
      return findByDomId(doc, "li1").get_attributes()
    `);
    is(attrs.posinset, "1", "li1 has correct posinset");
    is(attrs.setsize, "2", "li1 has correct setsize");
    is(attrs.level, "1", "li1 has correct level");

    attrs = await runPython(`findByDomId(doc, "li1a").get_attributes()`);
    is(attrs.posinset, "1", "li1a has correct posinset");
    is(attrs.setsize, "1", "li1a has correct setsize");
    is(attrs.level, "2", "li1a has correct level");

    attrs = await runPython(`findByDomId(doc, "li2").get_attributes()`);
    is(attrs.posinset, "2", "li2 has correct posinset");
    is(attrs.setsize, "2", "li2 has correct setsize");
    is(attrs.level, "1", "li2 has correct level");

    attrs = await runPython(`findByDomId(doc, "h2").get_attributes()`);
    ok(!("posinset" in attrs), "h2 doesn't have posinset");
    ok(!("setsize" in attrs), "h2 doesn't have setsize");
    is(attrs.level, "2", "h2 has correct level");

    attrs = await runPython(`findByDomId(doc, "button").get_attributes()`);
    ok(!("posinset" in attrs), "button doesn't have posinset");
    ok(!("setsize" in attrs), "button doesn't have setsize");
    ok(!("level" in attrs), "h2 doesn't have level");
  }
);
