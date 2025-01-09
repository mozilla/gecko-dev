/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

/**
 * Test accessibles aren't created for linebreaks.
 */
addAccessibleTask(
  `hello<br>world`,
  async (browser, accDoc) => {
    let doc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    let children = doc.getAttributeValue("AXChildren");
    is(children.length, 2, "The document contains 2 children");

    // verify first child is correct
    is(
      children[0].getAttributeValue("AXRole"),
      "AXStaticText",
      "First child is a text node"
    );
    is(
      children[0].getAttributeValue("AXValue"),
      "hello",
      "First child is hello text"
    );

    // verify second child is correct
    is(
      children[1].getAttributeValue("AXRole"),
      "AXStaticText",
      "Second child is a text node"
    );

    is(
      children[1].getAttributeValue("AXValue"),
      gIsIframe && !gIsRemoteIframe ? "world" : "world ",
      "Second child is world text"
    );
    // we have a trailing space in here due to bug 1577028
    // but this appears fixed in non-remote iframes
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

addAccessibleTask(
  `<p id="p">hello, this is a test</p>`,
  async (browser, accDoc) => {
    let p = getNativeInterface(accDoc, "p");
    let textLeaf = p.getAttributeValue("AXChildren")[0];
    ok(textLeaf, "paragraph has a text leaf");

    let str = textLeaf.getParameterizedAttributeValue(
      "AXStringForRange",
      NSRange(3, 6)
    );

    is(str, "lo, th", "AXStringForRange matches.");

    let smallBounds = textLeaf.getParameterizedAttributeValue(
      "AXBoundsForRange",
      NSRange(3, 6)
    );

    let largeBounds = textLeaf.getParameterizedAttributeValue(
      "AXBoundsForRange",
      NSRange(3, 8)
    );

    Assert.less(
      smallBounds.size[0],
      largeBounds.size[0],
      "longer range is wider"
    );
  },
  { chrome: true, iframe: true, remoteIframe: true }
);

// Text leaf inherits container's AXLanguage, and container's default AXLanguage is the doc one.
addAccessibleTask(
  `<p id="ar">العربية</p>
   <p id="es" lang="es">Español</p>`,
  async (browser, accDoc) => {
    const es = getNativeInterface(accDoc, "es");
    const ar = getNativeInterface(accDoc, "ar");
    const firstChild = a => a.getAttributeValue("AXChildren")[0];

    is(es.getAttributeValue("AXLanguage"), "es");
    is(firstChild(es).getAttributeValue("AXLanguage"), "es");

    is(ar.getAttributeValue("AXLanguage"), "ar");
    is(firstChild(ar).getAttributeValue("AXLanguage"), "ar");
  },
  { chrome: true, contentDocBodyAttrs: { lang: "ar" } }
);
