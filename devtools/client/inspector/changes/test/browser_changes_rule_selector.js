/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that renaming the selector of a CSS declaration in the Rule view is tracked as
// one rule removal with the old selector and one rule addition with the new selector.

const TEST_URI = `
  <style type='text/css'>
    div {
      color: red;
    }
  </style>
  <div></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const { document: doc, store } = selectChangesView(inspector);
  const panel = doc.querySelector("#sidebar-panel-changes");

  await selectNode("div", inspector);
  const ruleEditor = getRuleViewRuleEditor(ruleView, 1);

  info("Focusing the first rule's selector name in the Rule view");
  const editor = await focusEditableField(ruleView, ruleEditor.selectorText);
  info("Entering a new selector name");
  editor.input.value = ".test";

  // Expect two "TRACK_CHANGE" actions: one for rule removal, one for rule addition.
  const onTrackChange = waitUntilAction(store, "TRACK_CHANGE", 2);
  const onRuleViewChanged = once(ruleView, "ruleview-changed");
  info("Pressing Enter key to commit the change");
  EventUtils.synthesizeKey("KEY_Enter");
  info("Waiting for rule view to update");
  await onRuleViewChanged;
  info("Wait for the change to be tracked");
  await onTrackChange;

  const rules = panel.querySelectorAll(".rule");
  is(rules.length, 2, "Two rules were tracked as changed");

  const firstSelector = rules.item(0).querySelector(".selector");
  is(firstSelector.title, "div", "Old selector name was tracked.");
  ok(firstSelector.classList.contains("diff-remove"), "Old selector was removed.");

  const secondSelector = rules.item(1).querySelector(".selector");
  is(secondSelector.title, ".test", "New selector name was tracked.");
  ok(secondSelector.classList.contains("diff-add"), "New selector was added.");

  info("Checking that the two rules have identical declarations");
  const firstDecl = rules.item(0).querySelectorAll(".declaration");
  is(firstDecl.length, 1, "First rule has only one declaration");
  is(firstDecl.item(0).textContent, "color:red;", "First rule has correct declaration");
  ok(firstDecl.item(0).classList.contains("diff-remove"),
    "First rule has declaration tracked as removed");

  const secondDecl = rules.item(1).querySelectorAll(".declaration");
  is(secondDecl.length, 1, "Second rule has only one declaration");
  is(secondDecl.item(0).textContent, "color:red;", "Second rule has correct declaration");
  ok(secondDecl.item(0).classList.contains("diff-add"),
    "Second rule has declaration tracked as added");
});
