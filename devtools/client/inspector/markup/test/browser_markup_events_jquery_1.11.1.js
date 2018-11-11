/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */
"use strict";

// Test that markup view event bubbles show the correct event info for jQuery
// and jQuery Live events (jQuery version 1.11.1).

const TEST_LIB = "lib_jquery_1.11.1_min.js";
const TEST_URL = URL_ROOT + "doc_markup_events_jquery.html?" + TEST_LIB;

loadHelperScript("helper_events_test_runner.js");

/*eslint-disable */
const TEST_DATA = [
  {
    selector: "html",
    expected: [
      {
        type: "load",
        filename: TEST_URL + ":27",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "() => {\n" +
                 "  var handler1 = function liveDivDblClick() {\n" +
                 "    alert(1);\n" +
                 "  };\n" +
                 "  var handler2 = function liveDivDragStart() {\n" +
                 "    alert(2);\n" +
                 "  };\n" +
                 "  var handler3 = function liveDivDragLeave() {\n" +
                 "    alert(3);\n" +
                 "  };\n" +
                 "  var handler4 = function liveDivDragEnd() {\n" +
                 "    alert(4);\n" +
                 "  };\n" +
                 "  var handler5 = function liveDivDrop() {\n" +
                 "    alert(5);\n" +
                 "  };\n" +
                 "  var handler6 = function liveDivDragOver() {\n" +
                 "    alert(6);\n" +
                 "  };\n" +
                 "  var handler7 = function divClick1() {\n" +
                 "    alert(7);\n" +
                 "  };\n" +
                 "  var handler8 = function divClick2() {\n" +
                 "    alert(8);\n" +
                 "  };\n" +
                 "  var handler9 = function divKeyDown() {\n" +
                 "    alert(9);\n" +
                 "  };\n" +
                 "  var handler10 = function divDragOut() {\n" +
                 "    alert(10);\n" +
                 "  };\n" +
                 "\n" +
                 "  if ($(\"#livediv\").live) {\n" +
                 "    $(\"#livediv\").live(\"dblclick\", handler1);\n" +
                 "    $(\"#livediv\").live(\"dragstart\", handler2);\n" +
                 "  }\n" +
                 "\n" +
                 "  if ($(\"#livediv\").delegate) {\n" +
                 "    $(document).delegate(\"#livediv\", \"dragleave\", handler3);\n" +
                 "    $(document).delegate(\"#livediv\", \"dragend\", handler4);\n" +
                 "  }\n" +
                 "\n" +
                 "  if ($(\"#livediv\").on) {\n" +
                 "    $(document).on(\"drop\", \"#livediv\", handler5);\n" +
                 "    $(document).on(\"dragover\", \"#livediv\", handler6);\n" +
                 "    $(document).on(\"dragout\", \"#livediv:xxxxx\", handler10);\n" +
                 "  }\n" +
                 "\n" +
                 "  var div = $(\"div\")[0];\n" +
                 "  $(div).click(handler7);\n" +
                 "  $(div).click(handler8);\n" +
                 "  $(div).keydown(handler9);\n" +
                 "}"
      }
    ]
  },

  {
    selector: "#testdiv",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":34",
        attributes: [
          "jQuery"
        ],
        handler: "var handler7 = function divClick1() {\n" +
                 "  alert(7);\n" +
                 "}"
      },
      {
        type: "click",
        filename: TEST_URL + ":35",
        attributes: [
          "jQuery"
        ],
        handler: "var handler8 = function divClick2() {\n" +
                 "  alert(8);\n" +
                 "}"
      },
      {
        type: "click",
        filename: URL_ROOT + TEST_LIB + ":3",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "k = r.handle = function(a) {\n" +
                 "  return typeof m === K || a && m.event.triggered === a.type ? void 0 : m.event.dispatch.apply(k.elem, arguments)\n" +
                 "}"
      },
      {
        type: "keydown",
        filename: TEST_URL + ":36",
        attributes: [
          "jQuery"
        ],
        handler: "var handler9 = function divKeyDown() {\n" +
                 "  alert(9);\n" +
                 "}"
      },
      {
        type: "keydown",
        filename: URL_ROOT + TEST_LIB + ":3",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "k = r.handle = function(a) {\n" +
                 "  return typeof m === K || a && m.event.triggered === a.type ? void 0 : m.event.dispatch.apply(k.elem, arguments)\n" +
                 "}"
      }
    ]
  },

  {
    selector: "#livediv",
    expected: [
      {
        type: "dragend",
        filename: TEST_URL + ":31",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "var handler4 = function liveDivDragEnd() {\n" +
                 "  alert(4);\n" +
                 "}"
      },
      {
        type: "dragleave",
        filename: TEST_URL + ":30",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "var handler3 = function liveDivDragLeave() {\n" +
                 "  alert(3);\n" +
                 "}"
      },
      {
        type: "dragover",
        filename: TEST_URL + ":33",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "var handler6 = function liveDivDragOver() {\n" +
                 "  alert(6);\n" +
                 "}"
      },
      {
        type: "drop",
        filename: TEST_URL + ":32",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "var handler5 = function liveDivDrop() {\n" +
                 "  alert(5);\n" +
                 "}"
      }
    ]
  },
];
/*eslint-enable */

add_task(function* () {
  yield runEventPopupTests(TEST_URL, TEST_DATA);
});
