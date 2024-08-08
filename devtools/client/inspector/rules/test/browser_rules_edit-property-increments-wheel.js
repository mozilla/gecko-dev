/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that increasing/decreasing values in rule view using
// the mouse wheel works correctly.

const TEST_URI = `
  <style>
    #test {
      margin-top: 0px;
    }
  </style>
  <div id="test"></div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { inspector, view } = await openRuleView();
  await selectNode("#test", inspector);

  info("Testing wheel increments on the margin property");

  const marginPropEditor = getTextProperty(view, 1, {
    "margin-top": "0px",
  }).editor;

  await runWheelIncrementTest(marginPropEditor, view, { horizontal: false });
  await runWheelIncrementTest(marginPropEditor, view, { horizontal: true });
});

function runWheelIncrementTest(marginPropEditor, view, { horizontal }) {
  const wheelDelta = horizontal ? "deltaX" : "deltaY";
  return runIncrementTest(marginPropEditor, view, {
    1: {
      wheel: true,
      [wheelDelta]: -1,
      ...getSmallIncrementKey(),
      start: "0px",
      end: "0.1px",
      selectAll: true,
    },
    2: {
      wheel: true,
      [wheelDelta]: -1,
      start: "0px",
      end: "1px",
      selectAll: true,
    },
    3: {
      wheel: true,
      [wheelDelta]: -1,
      shift: true,
      start: "0px",
      end: "10px",
      selectAll: true,
    },
    4: {
      wheel: true,
      [wheelDelta]: 1,
      ...getSmallIncrementKey(),
      start: "0.1px",
      end: "0px",
      selectAll: true,
    },
    5: {
      wheel: true,
      [wheelDelta]: 1,
      down: true,
      start: "0px",
      end: "-1px",
      selectAll: true,
    },
    6: {
      wheel: true,
      [wheelDelta]: 1,
      down: true,
      shift: true,
      start: "0px",
      end: "-10px",
      selectAll: true,
    },
    7: {
      wheel: true,
      [wheelDelta]: -1,
      start: "0",
      end: "1px",
      selectAll: true,
    },
    8: {
      wheel: true,
      [wheelDelta]: 1,
      down: true,
      start: "0",
      end: "-1px",
      selectAll: true,
    },
  });
}
