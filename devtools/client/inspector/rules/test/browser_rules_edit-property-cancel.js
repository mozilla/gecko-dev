/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests editing a property name or value and escaping will revert the
// changes and restore the original value.

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: #00F;
  }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView({ overrideDebounce: false });
  await selectNode("#testid", inspector);

  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const prop = getTextProperty(view, 1, { "background-color": "#00F" });
  const propEditor = prop.editor;

  await focusEditableField(view, propEditor.nameSpan);
  await sendKeysAndWaitForFocus(view, ruleEditor.element, ["DELETE", "ESCAPE"]);

  is(
    propEditor.nameSpan.textContent,
    "background-color",
    "'background-color' property name is correctly set."
  );
  is(
    await getComputedStyleProperty("#testid", null, "background-color"),
    "rgb(0, 0, 255)",
    "#00F background color is set."
  );

  const editor = await focusEditableField(view, propEditor.valueSpan);
  info("Delete input value");
  const onValueDeleted = view.once("ruleview-changed");
  EventUtils.sendKey("DELETE", view.styleWindow);
  await onValueDeleted;

  is(editor.input.value, "", "value input is empty");

  await waitFor(() => view.popup?.isOpen);
  ok(true, "autocomplete popup opened");

  info("Hide autocomplete popup");
  const onPopupClosed = once(view.popup, "popup-closed");
  EventUtils.sendKey("ESCAPE", view.styleWindow);
  await onPopupClosed;
  ok(true, "popup was closed");

  info("Cancel edit with escape key");
  const onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.sendKey("ESCAPE", view.styleWindow);
  await onRuleViewChanged;

  is(
    propEditor.valueSpan.textContent,
    "#00F",
    "'#00F' property value is correctly set."
  );
  is(
    await getComputedStyleProperty("#testid", null, "background-color"),
    "rgb(0, 0, 255)",
    "#00F background color is set."
  );

  is(
    propEditor.warning.hidden,
    true,
    "warning icon is hidden after cancelling the edit"
  );
});
