/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../framework/test/shared-head.js */
/* import-globals-from ../../test/head.js */
"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this);

Services.prefs.setIntPref("devtools.toolbox.footer.height", 350);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.toolbox.footer.height");
});

/**
 * Highlight a node and set the inspector's current selection to the node or
 * the first match of the given css selector.
 * @param {String|NodeFront} selectorOrNodeFront
 *        The selector for the node to be set, or the nodeFront
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @return a promise that resolves when the inspector is updated with the new
 * node
 */
function* selectAndHighlightNode(selectorOrNodeFront, inspector) {
  info("Highlighting and selecting the node " + selectorOrNodeFront);

  let nodeFront = yield getNodeFront(selectorOrNodeFront, inspector);
  let updated = inspector.toolbox.once("highlighter-ready");
  inspector.selection.setNodeFront(nodeFront, "test-highlight");
  yield updated;
}

/**
 * Open the toolbox, with the inspector tool visible, and the computed view
 * sidebar tab selected to display the box model view.
 * @return a promise that resolves when the inspector is ready and the box model
 * view is visible and ready
 */
function openBoxModelView() {
  return openInspectorSidebarTab("computedview").then(data => {
    // The actual highligher show/hide methods are mocked in box model tests.
    // The highlighter is tested in devtools/inspector/test.
    function mockHighlighter({highlighter}) {
      highlighter.showBoxModel = function () {
        return promise.resolve();
      };
      highlighter.hideBoxModel = function () {
        return promise.resolve();
      };
    }
    mockHighlighter(data.toolbox);

    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      view: data.inspector.computedview.boxModelView,
      testActor: data.testActor
    };
  });
}

/**
 * Wait for the boxmodel-view-updated event.
 * @return a promise
 */
function waitForUpdate(inspector) {
  return inspector.once("boxmodel-view-updated");
}

function getStyle(testActor, selector, propertyName) {
  return testActor.eval(`
    content.document.querySelector("${selector}")
                    .style.getPropertyValue("${propertyName}");
  `);
}

function setStyle(testActor, selector, propertyName, value) {
  return testActor.eval(`
    content.document.querySelector("${selector}")
                    .style.${propertyName} = "${value}";
  `);
}
