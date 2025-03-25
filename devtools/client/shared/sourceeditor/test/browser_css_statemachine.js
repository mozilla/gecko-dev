/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CSSCompleter = require("resource://devtools/client/shared/sourceeditor/css-autocompleter.js");
const {
  cssTokenizerWithLineColumn,
} = require("resource://devtools/shared/css/parsing-utils.js");

const CSS_URI =
  "http://mochi.test:8888/browser/devtools/client/shared/sourceeditor" +
  "/test/css_statemachine_testcases.css";

const source = read(CSS_URI);

// Test states to be tested for css state machine in css-autocompleter.js file.",
// Test cases are of the following format:",
// [
//   [
//     line, // The line location of the cursor
//     ch    // The column locaiton of the cursor
//   ],
//   [
//     state,         // one of CSSCompleter.CSS_STATE_*
//     selectorState, // one of CSSCompleter.CSS_SELECTOR_STATE_*
//     completing,    // what is being completed
//     propertyName,  // what property is being completed in case of value state
//                    // or the current selector that is being completed
//   ]
// ]
const tests = [
  [
    [0, 10],
    [CSSCompleter.CSS_STATE_NULL, "", "", ""],
  ],
  [
    [4, 3],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_CLASS,
      "de",
      ".de",
    ],
  ],
  [
    [5, 8],
    [
      CSSCompleter.CSS_STATE_PROPERTY,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "-moz-a",
    ],
  ],
  [
    [5, 21],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "no",
      "-moz-appearance",
    ],
  ],
  [
    [6, 18],
    [
      CSSCompleter.CSS_STATE_PROPERTY,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "padding",
    ],
  ],
  [
    [6, 24],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "3",
      "padding",
    ],
  ],
  [
    [6, 29],
    [
      CSSCompleter.CSS_STATE_PROPERTY,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "bo",
    ],
  ],
  [
    [6, 50],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "1p",
      "border-bottom-width",
    ],
  ],
  [
    [7, 24],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "s",
      "border-bottom-style",
    ],
  ],
  [
    [9, 0],
    [CSSCompleter.CSS_STATE_NULL, CSSCompleter.CSS_SELECTOR_STATE_NULL, "", ""],
  ],
  [
    [10, 6],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_ID,
      "devto",
      "#devto",
    ],
  ],
  [
    [10, 17],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_CLASS,
      "de",
      "#devtools-menu.de",
    ],
  ],
  [
    [11, 5],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_CLASS,
      "devt",
      ".devt",
    ],
  ],
  [
    [11, 30],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_ID,
      "devtoo",
      ".devtools-toolbarbutton#devtoo",
    ],
  ],
  [
    [12, 10],
    [
      CSSCompleter.CSS_STATE_PROPERTY,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "-moz-app",
    ],
  ],
  [
    [16, 27],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "hsl",
      "text-shadow",
    ],
  ],
  [
    [19, 24],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "linear-gra",
      "background",
    ],
  ],
  [
    [19, 55],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "hsl",
      "background",
    ],
  ],
  [
    [19, 79],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "paddin",
      "background",
    ],
  ],
  [
    [20, 47],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "ins",
      "box-shadow",
    ],
  ],
  [
    [22, 15],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "inheri",
      "color",
    ],
  ],
  [
    [25, 26],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "",
      ".devtools-toolbarbutton > ",
    ],
  ],
  [
    [25, 28],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_TAG,
      "hb",
      ".devtools-toolbarbutton > hb",
    ],
  ],
  [
    [25, 41],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_CLASS,
      "toolbarbut",
      ".devtools-toolbarbutton > hbox.toolbarbut",
    ],
  ],
  [
    [29, 21],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_PSEUDO,
      "ac",
      ".devtools-menulist:ac",
    ],
  ],
  [
    [30, 27],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_PSEUDO,
      "foc",
      "#devtools-toolbarbutton:foc",
    ],
  ],
  [
    [31, 18],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "dot",
      "outline",
    ],
  ],
  [
    [32, 25],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "-4p",
      "outline-offset",
    ],
  ],
  [
    [35, 26],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_PSEUDO,
      "no",
      ".devtools-toolbarbutton:no",
    ],
  ],
  [
    [35, 28],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "not",
      "",
    ],
  ],
  [
    [35, 30],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_ATTRIBUTE,
      "l",
      "[l",
    ],
  ],
  [
    [39, 46],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_CLASS,
      "toolba",
      ".devtools-toolbarbutton:not([label]) > .toolba",
    ],
  ],
  [
    [43, 39],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_VALUE,
      "tr",
      "[checked=tr",
    ],
  ],
  [
    [43, 47],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_PSEUDO,
      "hov",
      ".devtools-toolbarbutton:not([checked=true]):hov",
    ],
  ],
  [
    [43, 53],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_PSEUDO,
      "act",
      ".devtools-toolbarbutton:not([checked=true]):hover:act",
    ],
  ],
  [
    [47, 22],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_ATTRIBUTE,
      "op",
      ".devtools-menulist[op",
    ],
  ],
  [
    [47, 33],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_VALUE,
      "tr",
      ".devtools-menulist[open =tr",
    ],
  ],
  [
    [48, 38],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_VALUE,
      "tr",
      ".devtools-toolbarbutton[open = tr",
    ],
  ],
  [
    [49, 40],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_VALUE,
      "true",
      ".devtools-toolbarbutton[checked= true",
    ],
  ],
  [
    [53, 34],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_VALUE,
      "=",
      ".devtools-toolbarbutton[checked=",
    ],
  ],
  [
    [58, 38],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "!impor",
      "background-color",
    ],
  ],
  [
    [61, 41],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_PSEUDO,
      "hov",
      ".devtools-toolbarbutton[checked=true]:hov",
    ],
  ],
  [
    [65, 47],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_CLASS,
      "to",
      ".devtools-toolbarbutton[type=menu-button] > .to",
    ],
  ],
  [
    [69, 44],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_PSEUDO,
      "first-of",
      ".devtools-sidebar-tabs > tabs > tab:first-of",
    ],
  ],
  [
    [73, 45],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_PSEUDO,
      "last",
      ":last",
    ],
  ],
  [
    [77, 27],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_CLASS,
      "vis",
      ".vis",
    ],
  ],
  [
    [78, 34],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_CLASS,
      "hidd",
      ".hidden-labels-box.visible ~ .hidd",
    ],
  ],
  [
    [83, 5],
    [
      CSSCompleter.CSS_STATE_MEDIA,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "medi",
    ],
  ],
  [
    [83, 22],
    [CSSCompleter.CSS_STATE_MEDIA, CSSCompleter.CSS_SELECTOR_STATE_NULL, "800"],
  ],
  [
    [84, 9],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_CLASS,
      "catego",
      ".catego",
    ],
  ],
  [
    [89, 9],
    [CSSCompleter.CSS_STATE_MEDIA, CSSCompleter.CSS_SELECTOR_STATE_NULL, "al"],
  ],
  [
    [90, 6],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_ID,
      "err",
      "#err",
    ],
  ],
  [
    [93, 11],
    [
      CSSCompleter.CSS_STATE_PROPERTY,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "backgro",
    ],
  ],
  [
    [98, 6],
    [
      CSSCompleter.CSS_STATE_SELECTOR,
      CSSCompleter.CSS_SELECTOR_STATE_TAG,
      "butt",
      "butt",
    ],
  ],
  [
    [99, 22],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "!impor",
      "width",
    ],
  ],
  [
    [103, 5],
    [
      CSSCompleter.CSS_STATE_KEYFRAMES,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "ke",
    ],
  ],
  [
    [104, 7],
    [CSSCompleter.CSS_STATE_FRAME, CSSCompleter.CSS_SELECTOR_STATE_NULL, "fro"],
  ],
  [
    [104, 15],
    [
      CSSCompleter.CSS_STATE_PROPERTY,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "opac",
    ],
  ],
  [
    [104, 29],
    [
      CSSCompleter.CSS_STATE_PROPERTY,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "transf",
    ],
  ],
  [
    [104, 38],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "scal",
      "transform",
    ],
  ],
  [
    [105, 8],
    [CSSCompleter.CSS_STATE_FRAME, CSSCompleter.CSS_SELECTOR_STATE_NULL, ""],
  ],
  [
    [113, 6],
    [
      CSSCompleter.CSS_STATE_KEYFRAMES,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "keyfr",
    ],
  ],
  [
    [114, 4],
    [CSSCompleter.CSS_STATE_FRAME, CSSCompleter.CSS_SELECTOR_STATE_NULL, "fr"],
  ],
  [
    [115, 3],
    [CSSCompleter.CSS_STATE_FRAME, CSSCompleter.CSS_SELECTOR_STATE_NULL, "2"],
  ],
  [
    [117, 8],
    [
      CSSCompleter.CSS_STATE_PROPERTY,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "opac",
    ],
  ],
  [
    [117, 16],
    [
      CSSCompleter.CSS_STATE_VALUE,
      CSSCompleter.CSS_SELECTOR_STATE_NULL,
      "0",
      "opacity",
    ],
  ],
  [
    [121, 0],
    [CSSCompleter.CSS_STATE_NULL, "", ""],
  ],
];

const TEST_URI =
  "data:text/html;charset=UTF-8," +
  encodeURIComponent(`
    <!DOCTYPE html>
    <html>
      <head><title>CSS State machine tests.</title></head>
      <body>
        <h2>State machine tests for CSS autocompleter.</h2>
      </body>
    </html>
  `);

add_task(async function test() {
  await addTab(TEST_URI);

  const completer = new CSSCompleter({
    cssProperties: getClientCssProperties(),
  });

  let i = 0;
  for (const testcase of tests) {
    ++i;
    // if (i !== 2) continue;
    const [[line, column], expected] = testcase;
    const limitedSource = limit(source, [line, column]);

    info(`Test case ${i} from source`);
    completer.resolveState({
      source: limitedSource,
      line,
      column,
    });
    assertState(completer, expected, i + " (from_source)");

    info(`Test case ${i} from tokens`);
    completer.resolveState({
      sourceTokens: cssTokenizerWithLineColumn(limitedSource),
    });
    assertState(completer, expected, i + " (from tokens)");
  }
  gBrowser.removeCurrentTab();
});

function assertState(completer, expected, testCaseName) {
  if (checkState(completer, expected)) {
    ok(true, `Test ${testCaseName} passed. `);
  } else {
    ok(
      false,
      `Test ${testCaseName} failed. Expected state : ${JSON.stringify([
        expected[0]?.toString(),
        expected[1]?.toString(),
        expected[2],
        expected[3],
      ])} but found ${JSON.stringify([
        completer.state?.toString(),
        completer.selectorState?.toString(),
        completer.completing,
        completer.propertyName || completer.selector,
      ])}.`
    );
  }
}

function checkState(completer, expected) {
  if (
    expected[0] == CSSCompleter.CSS_STATE_NULL &&
    (!completer.state || completer.state == CSSCompleter.CSS_STATE_NULL)
  ) {
    return true;
  } else if (
    expected[0] == completer.state &&
    expected[0] == CSSCompleter.CSS_STATE_SELECTOR &&
    expected[1] == completer.selectorState &&
    expected[2] == completer.completing &&
    expected[3] == completer.selector
  ) {
    return true;
  } else if (
    expected[0] == completer.state &&
    expected[0] == CSSCompleter.CSS_STATE_VALUE &&
    expected[2] == completer.completing &&
    expected[3] == completer.propertyName
  ) {
    return true;
  } else if (
    expected[0] == completer.state &&
    expected[2] == completer.completing &&
    expected[0] != CSSCompleter.CSS_STATE_SELECTOR &&
    expected[0] != CSSCompleter.CSS_STATE_VALUE
  ) {
    return true;
  }
  return false;
}
