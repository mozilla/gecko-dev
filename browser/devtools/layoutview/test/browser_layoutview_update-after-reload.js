/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the layout-view continues to work after the page is reloaded

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_layoutview_iframe1.html");
  let {toolbox, inspector, view} = yield openLayoutView();

  info("Test that the layout-view works on the first page");
  yield assertLayoutView(inspector, view);

  info("Reload the page");
  content.location.reload();
  yield inspector.once("markuploaded");

  info("Test that the layout-view works on the reloaded page");
  yield assertLayoutView(inspector, view);
});

function* assertLayoutView(inspector, view) {
  info("Selecting the test node");
  yield selectNode("p", inspector);

  info("Checking that the layout-view shows the right value");
  let paddingElt = view.doc.querySelector(".padding.top > span");
  is(paddingElt.textContent, "50");

  info("Listening for layout-view changes and modifying the padding");
  let onUpdated = waitForUpdate(inspector);
  getNode("p").style.padding = "20px";
  yield onUpdated;
  ok(true, "Layout-view got updated");

  info("Checking that the layout-view shows the right value after update");
  is(paddingElt.textContent, "20");
}
