/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test void element display in the markupview.
const TEST_URL = URL_ROOT + "doc_markup_void_elements.html";

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);
  let {win} = inspector.markup;

  info("check non-void element closing tag is displayed");
  let {editor} = yield getContainerForSelector("h1", inspector);
  ok(!editor.elt.classList.contains("void-element"),
    "h1 element does not have void-element class");
  ok(!editor.elt.querySelector(".close").style.display !== "none",
    "h1 element tag is not hidden");

  info("check void element closing tag is hidden in HTML document");
  let container = yield getContainerForSelector("img", inspector);
  ok(container.editor.elt.classList.contains("void-element"),
    "img element has the expected class");
  let closeElement = container.editor.elt.querySelector(".close");
  let computedStyle = win.getComputedStyle(closeElement, null);
  ok(computedStyle.display === "none", "img closing tag is hidden");

  info("check void element with pseudo element");
  let hrNodeFront = yield getNodeFront("hr.before", inspector);
  container = getContainerForNodeFront(hrNodeFront, inspector);
  ok(container.editor.elt.classList.contains("void-element"),
    "hr element has the expected class");
  closeElement = container.editor.elt.querySelector(".close");
  computedStyle = win.getComputedStyle(closeElement, null);
  ok(computedStyle.display === "none", "hr closing tag is hidden");

  info("check expanded void element closing tag is not hidden");
  yield inspector.markup.expandNode(hrNodeFront);
  yield waitForMultipleChildrenUpdates(inspector);
  ok(container.expanded, "hr container is expanded");
  computedStyle = win.getComputedStyle(closeElement, null);
  ok(computedStyle.display === "none", "hr closing tag is not hidden anymore");
});
