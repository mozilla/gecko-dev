/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view marks overridden rules correctly based on the
// order of the property.

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: green;
  }
  </style>
  <div id='testid' class='testclass'>Styled Node</div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  let rule = getRuleViewRuleEditor(view, 1).rule;

  yield addProperty(view, 1, "background-color", "red");

  let firstProp = rule.textProps[0];
  let secondProp = rule.textProps[1];

  ok(firstProp.overridden, "First property should be overridden.");
  ok(!secondProp.overridden, "Second property should not be overridden.");
});
