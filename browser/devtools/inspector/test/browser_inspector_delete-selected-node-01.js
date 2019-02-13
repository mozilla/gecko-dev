/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test to ensure inspector handles deletion of selected node correctly.

const TEST_URL = TEST_URL_ROOT + "doc_inspector_delete-selected-node-01.html";

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);

  let span = yield getNodeFrontInFrame("span", "iframe", inspector);
  yield selectNode(span, inspector);

  info("Removing selected <span> element.");
  let parentNode = span.parentNode();
  yield inspector.walker.removeNode(span);

  // Wait for the inspector to process the mutation
  yield inspector.once("inspector-updated");
  is(inspector.selection.nodeFront, parentNode,
    "Parent node of selected <span> got selected.");
});
