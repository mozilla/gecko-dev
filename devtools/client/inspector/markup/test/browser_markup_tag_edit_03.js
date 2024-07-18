/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a node's tagname can be edited in the markup-view

const TEST_URL = `data:text/html;charset=utf-8,
                  <div id='retag-me'><div id='retag-me-2'></div></div>`;

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);

  await inspector.markup.expandAll();

  info("Selecting the test node");
  await focusNode("#retag-me", inspector);

  info("Getting the markup-container for the test node");
  let container = await getRetagMeContainer(inspector);
  ok(container.expanded, "The container is expanded");

  is(
    (await getContentPageElementProperty("#retag-me", "tagName")).toLowerCase(),
    "div",
    "We've got #retag-me element, it's a DIV"
  );
  is(
    await getContentPageElementProperty("#retag-me", "childElementCount"),
    1,
    "#retag-me has one child"
  );

  is(
    await getContentPageElementProperty("#retag-me > *", "id"),
    "retag-me-2",
    "#retag-me's only child is #retag-me-2"
  );

  info("Changing #retag-me's tagname in the markup-view");
  await setRetagMeTagnameValue(inspector, "p", {
    waitForSelection: true,
  });

  info("Checking that the tagname change was done");
  is(
    (await getContentPageElementProperty("#retag-me", "tagName")).toLowerCase(),
    "p",
    "The #retag-me element is now a P"
  );
  is(
    await getContentPageElementProperty("#retag-me", "childElementCount"),
    1,
    "#retag-me still has one child"
  );
  is(
    await getContentPageElementProperty("#retag-me > *", "id"),
    "retag-me-2",
    "#retag-me's only child is #retag-me-2"
  );
  info("Checking that the markup-container exists and is correct");
  container = await getRetagMeContainer(inspector);
  ok(container.expanded, "The container is still expanded");
  ok(container.selected, "The container is still selected");

  info("Add attributes through tagname input");
  // tagName is `p` at this point, let's keep it as is for now
  await setRetagMeTagnameValue(
    inspector,
    `p data-x="hello world" class=my-attr readonly`
  );

  info("Checking that attributes were added");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const el = content.document.getElementById("retag-me");
    is(el.tagName.toLowerCase(), "p", "element tagName is still the same");
    is(el.getAttribute("class"), "my-attr", "class attribute was set");
    is(el.getAttribute("data-x"), "hello world", "data-x attribute was set");
    ok(el.hasAttribute("readonly"), "readonly attribute was set");
  });

  info(
    "Change tagName, add attributes and override others, all through tagname input"
  );
  // tagName is `p` at this point, let's change it, as well as `class` value
  await setRetagMeTagnameValue(inspector, `main class=my-attr-2`, {
    waitForSelection: true,
  });

  info("Checking that attributes were added");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const el = content.document.getElementById("retag-me");
    is(el.tagName.toLowerCase(), "main", "element tagName was changed to main");
    is(el.getAttribute("class"), "my-attr-2", "class attribute was updated");
    is(el.getAttribute("data-x"), "hello world", "data-x attribute was kept");
    ok(el.hasAttribute("readonly"), "readonly attribute was kept");
  });

  info("Only change attributes again so we can check that undo works");
  // tagName is `main` at this point we want to keep it, and update attributes
  await setRetagMeTagnameValue(inspector, `main class=my-attr-3 new-attr=true`);

  info("Checking that attributes were added");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const el = content.document.getElementById("retag-me");
    is(el.tagName.toLowerCase(), "main", "element tagName was not changed");
    is(el.getAttribute("class"), "my-attr-3", "class attribute was updated");
    is(el.getAttribute("new-attr"), "true", "new-attr attribute was added");
    is(el.getAttribute("data-x"), "hello world", "data-x attribute was kept");
    ok(el.hasAttribute("readonly"), "readonly attribute was kept");
  });

  info("Undo the change");
  await undoChange(inspector);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const el = content.document.getElementById("retag-me");
    is(el.tagName.toLowerCase(), "main", "element tagName was not changed");
    is(
      el.getAttribute("class"),
      "my-attr-2",
      "class attribute was changed back to its previous value"
    );
    ok(!el.hasAttribute("new-attr"), "new-attr attribute was removed");
    is(el.getAttribute("data-x"), "hello world", "data-x attribute was kept");
    ok(el.hasAttribute("readonly"), "readonly attribute was kept");
  });
});

async function getRetagMeContainer(inspector) {
  return waitFor(async () => {
    const nodeFront = await getNodeFront("#retag-me", inspector);
    const container = getContainerForNodeFront(nodeFront, inspector);
    if (!container || !container.expanded || !container.selected) {
      return false;
    }
    return container;
  });
}

async function setRetagMeTagnameValue(
  inspector,
  tagNameValue,
  { waitForSelection = false } = {}
) {
  const container = await getRetagMeContainer(inspector);
  const tagEditor = container.editor.tag;
  const mutated = inspector.once("markupmutation");
  const onReselectedOnRemoved = waitForSelection
    ? inspector.markup.once("reselectedonremoved")
    : null;
  setEditableFieldValue(tagEditor, tagNameValue, inspector);
  await mutated;
  ok(true, "markupmutation emited");
  if (waitForSelection) {
    info("waiting for reselectedonremoved event");
  }
  await onReselectedOnRemoved;
  if (waitForSelection) {
    ok(true, "reselectedonremoved event received");
  }
}
