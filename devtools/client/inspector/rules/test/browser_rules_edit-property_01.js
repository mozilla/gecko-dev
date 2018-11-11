/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing adding new properties via the inplace-editors in the rule
// view.
// FIXME: some of the inplace-editor focus/blur/commit/revert stuff
// should be factored out in head.js

const TEST_URI = `
  <style type="text/css">
  #testid {
    color: red;
    background-color: blue;
  }
  .testclass, .unmatched {
    background-color: green;
  }
  </style>
  <div id="testid" class="testclass">Styled Node</div>
  <div id="testid2">Styled Node</div>
`;

var BACKGROUND_IMAGE_URL = 'url("' + URL_ROOT + 'doc_test_image.png")';

var TEST_DATA = [
  { name: "border-color", value: "red", isValid: true },
  { name: "background-image", value: BACKGROUND_IMAGE_URL, isValid: true },
  { name: "border", value: "solid 1px foo", isValid: false },
];

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  let rule = getRuleViewRuleEditor(view, 1).rule;
  for (let {name, value, isValid} of TEST_DATA) {
    yield testEditProperty(view, rule, name, value, isValid);
  }
});

function* testEditProperty(view, rule, name, value, isValid) {
  info("Test editing existing property name/value fields");

  let doc = rule.editor.doc;
  let prop = rule.textProps[0];

  info("Focusing an existing property name in the rule-view");
  let editor = yield focusEditableField(view, prop.editor.nameSpan, 32, 1);

  is(inplaceEditor(prop.editor.nameSpan), editor,
    "The property name editor got focused");
  let input = editor.input;

  info("Entering a new property name, including : to commit and " +
    "focus the value");
  let onValueFocus = once(rule.editor.element, "focus", true);
  let onNameDone = view.once("ruleview-changed");
  EventUtils.sendString(name + ":", doc.defaultView);
  yield onValueFocus;
  yield onNameDone;

  // Getting the value editor after focus
  editor = inplaceEditor(doc.activeElement);
  input = editor.input;
  is(inplaceEditor(prop.editor.valueSpan), editor, "Focus moved to the value.");

  info("Entering a new value, including ; to commit and blur the value");
  let onValueDone = view.once("ruleview-changed");
  let onBlur = once(input, "blur");
  EventUtils.sendString(value + ";", doc.defaultView);
  yield onBlur;
  yield onValueDone;

  is(prop.editor.isValid(), isValid,
    value + " is " + isValid ? "valid" : "invalid");

  info("Checking that the style property was changed on the content page");
  let propValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name
  });

  if (isValid) {
    is(propValue, value, name + " should have been set.");
  } else {
    isnot(propValue, value, name + " shouldn't have been set.");
  }
}
