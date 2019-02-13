/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test several types of rule-view property edition

let TEST_URI = [
  '<style type="text/css">',
  '#testid {',
  '  background-color: blue;',
  '}',
  '.testclass, .unmatched {',
  '  background-color: green;',
  '}',
  '</style>',
  '<div id="testid" class="testclass">Styled Node</div>',
  '<div id="testid2">Styled Node</div>'
].join("\n");

add_task(function*() {
  let tab = yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  let {toolbox, inspector, view} = yield openRuleView();

  yield selectNode("#testid", inspector);
  yield testEditProperty(inspector, view);
  yield testDisableProperty(inspector, view);
  yield testPropertyStillMarkedDirty(inspector, view);
});

function* testEditProperty(inspector, ruleView) {
  let idRuleEditor = getRuleViewRuleEditor(ruleView, 1);
  let propEditor = idRuleEditor.rule.textProps[0].editor;

  let editor = yield focusEditableField(propEditor.nameSpan);
  let input = editor.input;
  is(inplaceEditor(propEditor.nameSpan), editor, "Next focused editor should be the name editor.");

  ok(input.selectionStart === 0 && input.selectionEnd === input.value.length, "Editor contents are selected.");

  // Try clicking on the editor's input again, shouldn't cause trouble (see bug 761665).
  EventUtils.synthesizeMouse(input, 1, 1, {}, ruleView.doc.defaultView);
  input.select();

  info("Entering property name \"border-color\" followed by a colon to focus the value");
  let onFocus = once(idRuleEditor.element, "focus", true);
  EventUtils.sendString("border-color:", ruleView.doc.defaultView);
  yield onFocus;
  yield idRuleEditor.rule._applyingModifications;

  info("Verifying that the focused field is the valueSpan");
  editor = inplaceEditor(ruleView.doc.activeElement);
  input = editor.input;
  is(inplaceEditor(propEditor.valueSpan), editor, "Focus should have moved to the value.");
  ok(input.selectionStart === 0 && input.selectionEnd === input.value.length, "Editor contents are selected.");

  info("Entering a value following by a semi-colon to commit it");
  let onBlur = once(editor.input, "blur");
  // Use sendChar() to pass each character as a string so that we can test propEditor.warning.hidden after each character.
  for (let ch of "red;") {
    EventUtils.sendChar(ch, ruleView.doc.defaultView);
    is(propEditor.warning.hidden, true,
      "warning triangle is hidden or shown as appropriate");
  }
  yield onBlur;
  yield idRuleEditor.rule._applyingModifications;

  let newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "border-color"
  });
  is(newValue, "red", "border-color should have been set.");

  info("Entering property name \"color\" followed by a colon to focus the value");
  onFocus = once(idRuleEditor.element, "focus", true);
  EventUtils.sendString("color:", ruleView.doc.defaultView);
  yield onFocus;

  info("Verifying that the focused field is the valueSpan");
  editor = inplaceEditor(ruleView.doc.activeElement);

  info("Entering a value following by a semi-colon to commit it");
  onBlur = once(editor.input, "blur");
  EventUtils.sendString("red;", ruleView.doc.defaultView);
  yield onBlur;
  yield idRuleEditor.rule._applyingModifications;

  let props = ruleView.element.querySelectorAll(".ruleview-property");
  for (let i = 0; i < props.length; i++) {
    is(props[i].hasAttribute("dirty"), i <= 1,
      "props[" + i + "] marked dirty as appropriate");
  }
}

function* testDisableProperty(inspector, ruleView) {
  let idRuleEditor = getRuleViewRuleEditor(ruleView, 1);
  let propEditor = idRuleEditor.rule.textProps[0].editor;

  info("Disabling a property");
  propEditor.enable.click();
  yield idRuleEditor.rule._applyingModifications;

  let newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "border-color"
  });
  is(newValue, "", "Border-color should have been unset.");

  info("Enabling the property again");
  propEditor.enable.click();
  yield idRuleEditor.rule._applyingModifications;

  newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "border-color"
  });
  is(newValue, "red", "Border-color should have been reset.");
}

function* testPropertyStillMarkedDirty(inspector, ruleView) {
  // Select an unstyled node.
  yield selectNode("#testid2", inspector);

  // Select the original node again.
  yield selectNode("#testid", inspector);

  let props = ruleView.element.querySelectorAll(".ruleview-property");
  for (let i = 0; i < props.length; i++) {
    is(props[i].hasAttribute("dirty"), i <= 1,
      "props[" + i + "] marked dirty as appropriate");
  }
}
