/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  info("Test Object type property started");

  const TEST_JSON_URL = `data:application/json,${JSON.stringify({
    x: {
      type: "string",
    },
    y: {
      items: [
        "a",
        "b",
        "c",
        "d",
        "e",
        "f",
        "g",
        "h",
        "i",
        "j",
        "k",
        "l",
        "m",
        "n",
      ],
      length: 5,
      a: 1,
      b: 2,
      c: 3,
      d: 4,
      e: 5,
    },
  })}`;
  await addJsonViewTab(TEST_JSON_URL);

  is(await getRowsCount(), 24, "The tree has the expected number of rows");

  is(await getRowText(0), `x: `, "The node starts expanded");

  info(`Collapse auto-expanded "x" node.`);
  await clickJsonNode(".jsonPanelBox .treeTable .treeLabel");

  is(
    await getRowsCount(),
    23,
    `The tree has one less row after collapsing "x"`
  );

  is(
    await getRowText(0),
    `x: { type: "string" }`,
    "The label must be indicating an object"
  );

  is(await getRowText(1), `y: `, "The y row is expanded at first");
  is(await getRowText(2), `items: `, "The items row is expanded");

  info("Collapse y.items");
  await clickJsonNode(".jsonPanelBox .treeTable tr:nth-of-type(3) .treeLabel");

  is(
    await getRowsCount(),
    9,
    `The tree has less rows after collapsing "y.items"`
  );
  is(
    await getRowText(2),
    `items: (14)[ "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", … ]`,
    "The items row is rendered as expected after being collapsed"
  );

  info("Collapse y");
  await clickJsonNode(".jsonPanelBox .treeTable tr:nth-of-type(2) .treeLabel");
  is(await getRowsCount(), 2, `The tree has only 2 rows after collapsing "y"`);
  is(
    await getRowText(1),
    `y: { length: 5, a: 1, b: 2, … }`,
    "The y row is rendered as expected after being collapsed"
  );
});

function getRowsCount() {
  return getElementCount(".jsonPanelBox .treeTable .treeRow");
}
