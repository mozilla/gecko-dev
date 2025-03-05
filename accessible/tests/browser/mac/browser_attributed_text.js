/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test read-only attributed strings
addAccessibleTask(
  `<h1>hello <a href="#" id="a1">world</a></h1>
   <p>this <b style="color: red; background-color: yellow;" aria-invalid="spelling">is</b> <span style="text-decoration: underline dotted green;">a</span> <a href="#" id="a2">test</a></p>`,
  async (browser, accDoc) => {
    let macDoc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );

    let range = macDoc.getParameterizedAttributeValue(
      "AXTextMarkerRangeForUnorderedTextMarkers",
      [
        macDoc.getAttributeValue("AXStartTextMarker"),
        macDoc.getAttributeValue("AXEndTextMarker"),
      ]
    );

    let attributedText = macDoc.getParameterizedAttributeValue(
      "AXAttributedStringForTextMarkerRange",
      range
    );

    let attributesList = attributedText.map(
      ({
        string,
        AXForegroundColor,
        AXBackgroundColor,
        AXUnderline,
        AXUnderlineColor,
        AXHeadingLevel,
        AXFont,
        AXLink,
        AXMarkedMisspelled,
      }) => [
        string,
        AXForegroundColor,
        AXBackgroundColor,
        AXUnderline,
        AXUnderlineColor,
        AXHeadingLevel,
        AXFont.AXFontSize,
        AXLink ? AXLink.getAttributeValue("AXDOMIdentifier") : null,
        AXMarkedMisspelled,
      ]
    );

    Assert.deepEqual(attributesList, [
      // string, fg color, bg color, underline, underline color, heading level, font size, link id, misspelled
      [
        "hello ",
        "#000000",
        "#ffffff",
        undefined,
        undefined,
        1,
        32,
        null,
        undefined,
      ],
      ["world", "#0066cc", "#ffffff", 1, "#0066cc", 1, 32, "a1", undefined],
      [
        "this ",
        "#000000",
        "#ffffff",
        undefined,
        undefined,
        undefined,
        16,
        null,
        undefined,
      ],
      [
        "is",
        "#ff0000",
        "#ffff00",
        undefined,
        undefined,
        undefined,
        16,
        null,
        1,
      ],
      [
        " ",
        "#000000",
        "#ffffff",
        undefined,
        undefined,
        undefined,
        16,
        null,
        undefined,
      ],
      ["a", "#000000", "#ffffff", 1, "#008000", undefined, 16, null, undefined],
      [
        " ",
        "#000000",
        "#ffffff",
        undefined,
        undefined,
        undefined,
        16,
        null,
        undefined,
      ],
      [
        "test",
        "#0066cc",
        "#ffffff",
        1,
        "#0066cc",
        undefined,
        16,
        "a2",
        undefined,
      ],
    ]);

    // Test different NSRange parameters for AXAttributedStringForRange
    let worldLeaf = findAccessibleChildByID(accDoc, "a1").firstChild;
    let wordStaticText = worldLeaf.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    attributedText = wordStaticText.getParameterizedAttributeValue(
      "AXAttributedStringForRange",
      NSRange(4, 1)
    );
    is(attributedText.length, 1, "Last character is in single attribute run");
    is(attributedText[0].string, "d", "Last character matches");

    attributedText = wordStaticText.getParameterizedAttributeValue(
      "AXAttributedStringForRange",
      NSRange(5, 1)
    );
    is(attributedText.length, 0, "Range is past accessible bounds");
  }
);

// Test misspelling in text area
addAccessibleTask(
  `<textarea id="t">hello worlf, i love you</textarea>`,
  async (browser, accDoc) => {
    let textArea = getNativeInterface(accDoc, "t");
    let spellDone = waitForEvent(EVENT_TEXT_ATTRIBUTE_CHANGED, "t");
    textArea.setAttributeValue("AXFocused", true);

    let attributedText = [];

    // For some internal reason we get several text attribute change events
    // before the attributed text returned provides the misspelling attributes.
    while (true) {
      await spellDone;

      let range = textArea.getAttributeValue("AXVisibleCharacterRange");
      attributedText = textArea.getParameterizedAttributeValue(
        "AXAttributedStringForRange",
        NSRange(...range)
      );

      if (attributedText.length != 3) {
        spellDone = waitForEvent(EVENT_TEXT_ATTRIBUTE_CHANGED, "t");
      } else {
        break;
      }
    }

    ok(attributedText[1].AXMarkedMisspelled);
  }
);

// Test getting a span of attributed text that includes an empty input element.
addAccessibleTask(
  `hello <input id="input"> world<button></button>`,
  async (browser, accDoc) => {
    let macDoc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );

    let range = macDoc.getParameterizedAttributeValue(
      "AXTextMarkerRangeForUnorderedTextMarkers",
      [
        macDoc.getAttributeValue("AXStartTextMarker"),
        macDoc.getAttributeValue("AXEndTextMarker"),
      ]
    );

    let attributedText = macDoc.getParameterizedAttributeValue(
      "AXAttributedStringForTextMarkerRange",
      range
    );

    let text = macDoc.getParameterizedAttributeValue(
      "AXStringForTextMarkerRange",
      range
    );

    is(
      attributedText.length,
      4,
      "Should be 4 attribute runs for 2 texts, input and button"
    );
    is(attributedText[0].string, `hello `, "Attributed string is correct");
    ok(
      !attributedText[0].AXAttachment,
      "Regular string attributes run doesn't have attachment"
    );
    is(
      attributedText[1].AXAttachment.getAttributeValue("AXRole"),
      "AXTextField",
      "Entry text attribute run has correct attachment"
    );
    is(
      attributedText[3].AXAttachment.getAttributeValue("AXRole"),
      "AXButton",
      "Button text attribute run has correct attachment"
    );
    is(text, `hello  world${kEmbedChar}`, "Unattributed string is correct");
  }
);

// Test text fragment.
addAccessibleTask(
  `This is a test.`,
  async (browser, accDoc) => {
    const macDoc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    const range = macDoc.getParameterizedAttributeValue(
      "AXTextMarkerRangeForUnorderedTextMarkers",
      [
        macDoc.getAttributeValue("AXStartTextMarker"),
        macDoc.getAttributeValue("AXEndTextMarker"),
      ]
    );
    const attributedText = macDoc.getParameterizedAttributeValue(
      "AXAttributedStringForTextMarkerRange",
      range
    );
    is(attributedText.length, 3);
    ok(!attributedText[0].AXHighlight);
    ok(attributedText[1].AXHighlight);
    is(attributedText[1].string, "test");
    ok(!attributedText[2].AXHighlight);
  },
  { urlSuffix: "#:~:text=test" }
);

// Test the <mark> element.
addAccessibleTask(
  `This is a <mark>test</mark>.`,
  async function testMark(browser, accDoc) {
    const macDoc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    const range = macDoc.getParameterizedAttributeValue(
      "AXTextMarkerRangeForUnorderedTextMarkers",
      [
        macDoc.getAttributeValue("AXStartTextMarker"),
        macDoc.getAttributeValue("AXEndTextMarker"),
      ]
    );
    const attributedText = macDoc.getParameterizedAttributeValue(
      "AXAttributedStringForTextMarkerRange",
      range
    );
    is(attributedText.length, 3);
    ok(!attributedText[0].AXHighlight);
    ok(attributedText[1].AXHighlight);
    is(attributedText[1].string, "test");
    ok(!attributedText[2].AXHighlight);
  }
);
