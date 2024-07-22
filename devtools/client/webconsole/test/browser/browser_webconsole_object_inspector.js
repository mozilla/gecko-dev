/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check expanding/collapsing object inspector in the console.
const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><h1>test Object Inspector</h1>";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  logAllStoreChanges(hud);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.console.log("oi-test", [1, 2, { a: "a", b: "b" }], {
      c: "c",
      d: [3, 4],
      length: 987,
    });
  });

  const node = await waitFor(() => findConsoleAPIMessage(hud, "oi-test"));
  const objectInspectors = [...node.querySelectorAll(".tree")];
  is(
    objectInspectors.length,
    2,
    "There is the expected number of object inspectors"
  );

  const [arrayOi, objectOi] = objectInspectors;

  await expandObjectInspectorNode(arrayOi.querySelector(".tree-node"));

  let arrayOiNodes = arrayOi.querySelectorAll(".tree-node");

  // The object inspector now looks like:
  // ▼ […]
  // |  0: 1
  // |  1: 2
  // |  ▶︎ 2: {a: "a", b: "b"}
  // |  length: 3
  // |  ▶︎ <prototype>
  is(
    arrayOiNodes.length,
    6,
    "There is the expected number of nodes in the tree"
  );

  info("Expanding a leaf of the array object inspector");
  let arrayOiNestedObject = arrayOiNodes[3];
  await expandObjectInspectorNode(arrayOiNestedObject);

  arrayOiNodes = arrayOi.querySelectorAll(".node");

  // The object inspector now looks like:
  // ▼ […]
  // |  0: 1
  // |  1: 2
  // |  ▼ 2: {…}
  // |  |  a: "a"
  // |  |  b: "b"
  // |  |  ▶︎ <prototype>
  // |  length: 3
  // |  ▶︎ <prototype>
  is(
    arrayOiNodes.length,
    9,
    "There is the expected number of nodes in the tree"
  );

  info("Collapsing the root");
  const onArrayOiMutation = waitForNodeMutation(arrayOi, {
    childList: true,
  });
  arrayOi.querySelector(".theme-twisty").click();
  await onArrayOiMutation;

  is(
    arrayOi.querySelector(".theme-twisty").classList.contains("open"),
    false,
    "The arrow of the root node of the tree is collapsed after clicking on it"
  );

  arrayOiNodes = arrayOi.querySelectorAll(".node");
  is(arrayOiNodes.length, 1, "Only the root node is visible");

  info("Expanding the root again");
  await expandObjectInspectorNode(arrayOi.querySelector(".tree-node"));

  arrayOiNodes = arrayOi.querySelectorAll(".node");
  arrayOiNestedObject = arrayOiNodes[3];
  ok(
    arrayOiNestedObject
      .querySelector(".theme-twisty")
      .classList.contains("open"),
    "The object tree is still expanded"
  );

  is(
    arrayOiNodes.length,
    9,
    "There is the expected number of nodes in the tree"
  );

  await expandObjectInspectorNode(objectOi.querySelector(".tree-node"));

  const objectOiNodes = objectOi.querySelectorAll(".node");
  // The object inspector now looks like:
  // ▼ {…}
  // |  c: "c"
  // |  ▶︎ d: [3, 4]
  // |  length: 987
  // |  ▶︎ <prototype>
  is(
    objectOiNodes.length,
    5,
    "There is the expected number of nodes in the tree"
  );
});
