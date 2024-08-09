/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test adding a valid property to a CSS rule, and navigating through the fields
// by pressing ENTER.

const TEST_URI = `
  <style type="text/css">
    #testid {
      color: blue;
    }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  info("Focus the new property name field");
  const ruleEditor = getRuleViewRuleEditor(view, 1);
  let editor = await focusNewRuleViewProperty(ruleEditor);
  const input = editor.input;

  is(
    inplaceEditor(ruleEditor.newPropSpan),
    editor,
    "Next focused editor should be the new property editor."
  );
  ok(
    input.selectionStart === 0 && input.selectionEnd === input.value.length,
    "Editor contents are selected."
  );

  // Try clicking on the editor's input again, shouldn't cause trouble
  // (see bug 761665).
  EventUtils.synthesizeMouse(input, 1, 1, {}, view.styleWindow);
  input.select();

  info("Entering the property name");
  editor.input.value = "background-color";

  info("Pressing RETURN and waiting for the value field focus");
  let onNameAdded = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);

  await onNameAdded;

  editor = inplaceEditor(view.styleDocument.activeElement);

  is(
    ruleEditor.rule.textProps.length,
    2,
    "Should have created a new text property."
  );
  is(
    ruleEditor.propertyList.children.length,
    2,
    "Should have created a property editor."
  );
  const textProp = ruleEditor.rule.textProps[1];
  is(
    editor,
    inplaceEditor(textProp.editor.valueSpan),
    "Should be editing the value span now."
  );

  info("Entering the property value");
  let onValueAdded = view.once("ruleview-changed");
  editor.input.value = "purple";
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  await onValueAdded;

  is(textProp.value, "purple", "Text prop should have been changed.");

  info("Test creating a new empty CSS variable");
  editor = await focusNewRuleViewProperty(ruleEditor);
  editor.input.value = "--x";

  info("Pressing RETURN and waiting for the value field focus");
  onNameAdded = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  await onNameAdded;

  info("Entering the empty property value");
  onValueAdded = view.once("ruleview-changed");
  // the input value should already be empty, but let's make it explicit
  inplaceEditor(view.styleDocument.activeElement).input.value = "";
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  await onValueAdded;

  is(
    ruleEditor.rule.textProps.length,
    3,
    "Should have created a new text property."
  );
  const emptyVarTextProp = ruleEditor.rule.textProps[2];
  is(emptyVarTextProp.value, "", "The empty variable was created.");
  ok(emptyVarTextProp.editor.isValid(), "The empty variable is valid.");
  is(
    emptyVarTextProp.editor.nameSpan.innerText,
    "--x",
    "Created expected declaration"
  );

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);

  info("Check that editing the variable name doesn't remove the declaration");
  await focusEditableField(view, emptyVarTextProp.editor.nameSpan);
  const onRuleViewChanged = view.once("ruleview-changed");
  const onTimeout = wait(500).then(() => "TIMEOUT");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  const raceResult = await Promise.race([onRuleViewChanged, onTimeout]);
  is(raceResult, "TIMEOUT", "ruleview-changed wasn't called");
  is(
    ruleEditor.rule.textProps.length,
    3,
    "We still have the same number of text properties."
  );
});
