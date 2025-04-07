/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that doesn't fit into any specific category.

const TEST_URL = `data:text/html;charset=utf8,
  <body>
    <div a b id='order' c class></div>
  <body>`;

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);

  await testOriginalAttributesOrder(inspector);
  await testOrderAfterAttributeChange(inspector);
  await testKeepFocusOnAddedSiblingNode(inspector);
});

async function testOriginalAttributesOrder(inspector) {
  info("Testing order of attributes on initial node render");

  const attributes = await getAttributesFromEditor("#order", inspector);
  Assert.deepEqual(
    attributes,
    ["id", "class", "a", "b", "c"],
    "ordered correctly"
  );
}

async function testOrderAfterAttributeChange(inspector) {
  info("Testing order of attributes after attribute is change by setAttribute");

  await setContentPageElementAttribute("#order", "a", "changed");

  const attributes = await getAttributesFromEditor("#order", inspector);
  Assert.deepEqual(
    attributes,
    ["id", "class", "a", "b", "c"],
    "order isn't changed"
  );
}

// Covers fix for Bug 1955040
async function testKeepFocusOnAddedSiblingNode(inspector) {
  info(
    "Test that when an attribute is being edited and a new sibling node is added, focus stays on the input"
  );
  await selectNode("#order", inspector);
  const container = await focusNode("#order", inspector);

  info("Edit id attribute");
  const idAttributeEl = container.editor.attrElements
    .get("id")
    .querySelector(".editable");
  idAttributeEl.focus();
  EventUtils.sendKey("return", inspector.panelWin);
  const inputEl = inplaceEditor(idAttributeEl).input;
  ok(inputEl, "Found editable field for editing id");

  is(inspector.markup.doc.activeElement, inputEl, "The input is focused");

  info("Add a sibling node");
  const nodeMutated = inspector.once("markupmutation");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.document.body.append(content.document.createTextNode("sibling"));
  });
  await nodeMutated;

  is(
    inspector.markup.doc.activeElement,
    inputEl,
    "The input is still focused after a sibling node was added"
  );
}
