/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly when modifying the
// existing search filter value

const SEARCH = "00F"

let TEST_URI = [
  '<style type="text/css">',
  '  #testid {',
  '    background-color: #00F;',
  '  }',
  '  .testclass {',
  '    width: 100%;',
  '  }',
  '</style>',
  '<div id="testid" class="testclass">Styled Node</div>'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testAddTextInFilter(inspector, view);
  yield testRemoveTextInFilter(inspector, view);
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
  is(ruleView.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(ruleView, 0).rule.selectorText, "element",
    "First rule is inline element.");

  let rule = getRuleViewRuleEditor(ruleView, 1).rule;

  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(rule.textProps[0].editor.container.classList.contains("ruleview-highlight"),
    "background-color text property is correctly highlighted.");
}

function* testRemoveTextInFilter(inspector, ruleView) {
  info("Press backspace and set filter text to \"00\"");

  let win = ruleView.doc.defaultView;
  let searchField = ruleView.searchField;
  let onRuleViewFiltered = inspector.once("ruleview-filtered");

  searchField.focus();
  EventUtils.synthesizeKey("VK_BACK_SPACE", {}, win);
  yield inspector.once("ruleview-filtered");

  info("Check that the correct rules are visible");
  is(ruleView.element.children.length, 3, "Should have 3 rules.");
  is(getRuleViewRuleEditor(ruleView, 0).rule.selectorText, "element",
    "First rule is inline element.");

  let rule = getRuleViewRuleEditor(ruleView, 1).rule;

  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(rule.textProps[0].editor.container.classList.contains("ruleview-highlight"),
    "background-color text property is correctly highlighted.");

  rule = getRuleViewRuleEditor(ruleView, 2).rule;

  is(rule.selectorText, ".testclass", "Second rule is .testclass.");
  ok(rule.textProps[0].editor.container.classList.contains("ruleview-highlight"),
    "width text property is correctly highlighted.");
}
