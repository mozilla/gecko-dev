/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that primitive value is shown

const TEST_URI = "data:text/html;charset=utf8,<!DOCTYPE html>";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.console.log(
      "wrapped-primitive-value-test",
      Object(123),
      Object(false),
      Object("")
    );
  });

  const node = await waitFor(() =>
    findConsoleAPIMessage(hud, "wrapped-primitive-value-test")
  );

  const objectInspectors = [...node.querySelectorAll(".tree")];

  is(
    objectInspectors.length,
    3,
    "There is the expected number of object inspectors"
  );
  const [intOi, boolOi, stringOi] = objectInspectors;
  await expandOiAndCheckPrimitiveValue(intOi, "123");
  await expandOiAndCheckPrimitiveValue(boolOi, "false");
  await expandOiAndCheckPrimitiveValue(stringOi, `""`);
});

async function expandOiAndCheckPrimitiveValue(oi, expectedPrimitiveValue) {
  info("Expanding the Object");
  await expandObjectInspectorNode(oi.querySelector(".tree-node"));

  const primitiveValueNode = [...getObjectInspectorNodes(oi)].find(nodes =>
    nodes.textContent.includes("<primitive value>")
  );

  ok(
    primitiveValueNode.textContent.includes(
      `<primitive value>: ${expectedPrimitiveValue}`
    ),
    "There is the expected <primitive value> node"
  );
}
