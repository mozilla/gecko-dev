/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that markup view event bubbles show the correct event info for DOM
// events.

const TEST_URL = URL_ROOT + "doc_markup_events_01.html";

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [ // eslint-disable-line
  {
    selector: "html",
    expected: [
      {
        type: "load",
        filename: TEST_URL,
        attributes: ["Bubbling", "DOM0"],
        handler: "function onload(event) {\n" + "  init();\n" + "}",
      },
    ],
  },
  {
    selector: "#container",
    expected: [
      {
        type: "mouseover",
        filename: TEST_URL + ":45:31",
        attributes: ["Capturing", "DOM2"],
        handler:
          "function mouseoverHandler(event) {\n" +
          '  if (event.target.id !== "container") {\n' +
          '    let output = document.getElementById("output");\n' +
          "    output.textContent = event.target.textContent;\n" +
          "  }\n" +
          "}",
      },
    ],
  },
  {
    selector: "#multiple",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":52:27",
        attributes: ["Bubbling", "DOM2"],
        handler:
          "function clickHandler(event) {\n" +
          '  let output = document.getElementById("output");\n' +
          '  output.textContent = "click";\n' +
          "}",
      },
      {
        type: "mouseup",
        filename: TEST_URL + ":57:29",
        attributes: ["Bubbling", "DOM2"],
        handler:
          "function mouseupHandler(event) {\n" +
          '  let output = document.getElementById("output");\n' +
          '  output.textContent = "mouseup";\n' +
          "}",
      },
    ],
  },
  // #noevents tests check that dynamically added events are properly displayed
  // in the markupview
  {
    selector: "#noevents",
    expected: [],
  },
  {
    selector: "#noevents",
    beforeTest: async function(inspector, testActor) {
      const nodeMutated = inspector.once("markupmutation");
      await testActor.eval("window.wrappedJSObject.addNoeventsClickHandler();");
      await nodeMutated;
    },
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":72:35",
        attributes: ["Bubbling", "DOM2"],
        handler:
          "function noeventsClickHandler(event) {\n" +
          '  alert("noevents has an event listener");\n' +
          "}",
      },
    ],
  },
  {
    selector: "#noevents",
    beforeTest: async function(inspector, testActor) {
      const nodeMutated = inspector.once("markupmutation");
      await testActor.eval(
        "window.wrappedJSObject.removeNoeventsClickHandler();"
      );
      await nodeMutated;
    },
    expected: [],
  },
  {
    selector: "#DOM0",
    expected: [
      {
        type: "click",
        filename: TEST_URL,
        attributes: ["Bubbling", "DOM0"],
        handler: "function onclick(event) {\n" + "  alert('DOM0')\n" + "}",
      },
    ],
  },
  {
    selector: "#handleevent",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":67:29",
        attributes: ["Bubbling", "DOM2"],
        handler: "function(blah) {\n" + '  alert("handleEvent");\n' + "}",
      },
    ],
  },
];

add_task(async function() {
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
