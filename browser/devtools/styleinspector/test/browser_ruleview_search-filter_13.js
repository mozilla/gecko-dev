/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter does not highlight the source with
// input that could be parsed as a property line.

const SEARCH = "doc_urls_clickable.css: url";
const TEST_URI = TEST_URL_ROOT + "doc_urls_clickable.html";

add_task(function*() {
  yield addTab(TEST_URI);
  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode(".relative1", inspector);
  yield testAddTextInFilter(inspector, view);
});

function* testAddTextInFilter(inspector, ruleView) {
  info("Setting filter text to \"" + SEARCH + "\"");

  let win = ruleView.doc.defaultView;
  let searchField = ruleView.searchField;
  let onRuleViewFiltered = inspector.once("ruleview-filtered");

  searchField.focus();
  synthesizeKeys(SEARCH, win);
  yield onRuleViewFiltered;

  info("Check that the correct rules are visible");
  is(ruleView.element.children.length, 1, "Should have 1 rules.");
  is(getRuleViewRuleEditor(ruleView, 0).rule.selectorText, "element",
    "First rule is inline element.");
}
