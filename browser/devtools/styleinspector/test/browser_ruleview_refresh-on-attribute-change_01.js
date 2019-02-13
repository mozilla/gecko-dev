/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changing the current element's attributes refreshes the rule-view

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,browser_ruleview_refresh-on-attribute-change.js");

  info("Preparing the test document and node");
  let style = '' +
    '#testid {' +
    '  background-color: blue;' +
    '} ' +
    '.testclass {' +
    '  background-color: green;' +
    '}';
  let styleNode = addStyle(content.document, style);
  content.document.body.innerHTML = '<div id="testid" class="testclass">Styled Node</div>';
  let testElement = getNode("#testid");
  let elementStyle = 'margin-top: 1px; padding-top: 5px;'
  testElement.setAttribute("style", elementStyle);

  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  info("Checking that the rule-view has the element, #testid and .testclass selectors");
  checkRuleViewContent(view, ["element", "#testid", ".testclass"]);

  info("Changing the node's ID attribute and waiting for the rule-view refresh");
  let ruleViewRefreshed = inspector.once("rule-view-refreshed");
  testElement.setAttribute("id", "differentid");
  yield ruleViewRefreshed;

  info("Checking that the rule-view doesn't have the #testid selector anymore");
  checkRuleViewContent(view, ["element", ".testclass"]);

  info("Reverting the ID attribute change");
  ruleViewRefreshed = inspector.once("rule-view-refreshed");
  testElement.setAttribute("id", "testid");
  yield ruleViewRefreshed;

  info("Checking that the rule-view has all the selectors again");
  checkRuleViewContent(view, ["element", "#testid", ".testclass"]);
});

function checkRuleViewContent(view, expectedSelectors) {
  let selectors = view.doc.querySelectorAll(".ruleview-selector");

  is(selectors.length, expectedSelectors.length,
    expectedSelectors.length + " selectors are displayed");

  for (let i = 0; i < expectedSelectors.length; i ++) {
    is(selectors[i].textContent.indexOf(expectedSelectors[i]), 0,
      "Selector " + (i + 1) + " is " + expectedSelectors[i]);
  }
}
