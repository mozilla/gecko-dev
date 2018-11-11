/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that inline inherited properties appear in the nested element.

var {ELEMENT_STYLE} = require("devtools/shared/specs/styles");

const TEST_URI = `
  <div id="test2" style="color: red">
    <div id="test1">Styled Node</div>
  </div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#test1", inspector);
  yield elementStyleInherit(inspector, view);
});

function* elementStyleInherit(inspector, view) {
  let elementStyle = view._elementStyle;
  is(elementStyle.rules.length, 2, "Should have 2 rules.");

  let elementRule = elementStyle.rules[0];
  ok(!elementRule.inherited,
    "Element style attribute should not consider itself inherited.");

  let inheritRule = elementStyle.rules[1];
  is(inheritRule.domRule.type, ELEMENT_STYLE,
    "Inherited rule should be an element style, not a rule.");
  ok(!!inheritRule.inherited, "Rule should consider itself inherited.");
  is(inheritRule.textProps.length, 1,
    "Should only display one inherited style");
  let inheritProp = inheritRule.textProps[0];
  is(inheritProp.name, "color", "color should have been inherited.");
}
