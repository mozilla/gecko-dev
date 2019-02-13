/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changes are previewed when editing a property value

// Format
// {
//   value : what to type in the field
//   expected : expected computed style on the targeted element
// }
const TEST_DATA = [
  {value: "inline", expected: "inline"},
  {value: "inline-block", expected: "inline-block"},

  // Invalid property values should not apply, and should fall back to default
  {value: "red", expected: "block"},
  {value: "something", expected: "block"},

  {escape: true, value: "inline", expected: "block"}
];

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,test rule view live preview on user changes");

  let style = '#testid {display:block;}';
  let styleNode = addStyle(content.document, style);
  content.document.body.innerHTML = '<div id="testid">Styled Node</div><span>inline element</span>';

  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  for (let data of TEST_DATA) {
    yield testLivePreviewData(data, view, "#testid");
  }
});

function* testLivePreviewData(data, ruleView, selector) {
  let testElement = getNode(selector);
  let idRuleEditor = getRuleViewRuleEditor(ruleView, 1);
  let propEditor = idRuleEditor.rule.textProps[0].editor;

  info("Focusing the property value inplace-editor");
  let editor = yield focusEditableField(propEditor.valueSpan);
  is(inplaceEditor(propEditor.valueSpan), editor, "The focused editor is the value");

  info("Enter a value in the editor")
  EventUtils.sendString(data.value, ruleView.doc.defaultView);
  if (data.escape) {
    EventUtils.synthesizeKey("VK_ESCAPE", {});
  } else {
    EventUtils.synthesizeKey("VK_RETURN", {});
  }

  // Wait for the modifyproperties request to complete before
  // checking the computed style.
  for (let rule of ruleView._elementStyle.rules) {
    if (rule._applyingModifications) {
      yield rule._applyingModifications;
    }
  }

  // While the editor is still focused in, the display should have changed already
  is((yield getComputedStyleProperty(selector, null, "display")),
    data.expected,
    "Element should be previewed as " + data.expected);
}
