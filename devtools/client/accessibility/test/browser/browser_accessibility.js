/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check basics of the accessibility panel

const TEST_URI = `data:text/html,<meta charset=utf8>
  <head>
    <title>TopLevel</title>
    <style>h1 { color: lightgrey; }</style>
  </head>
  <body>
    <h1>Top level header</h1>
    <p>This is a paragraph.</p>`;

add_task(async () => {
  const env = await addTestTab(TEST_URI);
  const { doc } = env;

  await checkTree(env, [
    {
      role: "document",
      name: `"TopLevel"`,
    },
  ]);

  ok(
    !doc.querySelector(".description"),
    "Before disabling the panel via the pref, the panel's description isn't visible"
  );

  await pushPref("accessibility.force_disabled", 1);
  await waitFor(
    () => doc.querySelector(".description"),
    "After disabling via the pref, the panel's description is visible"
  );

  await pushPref("accessibility.force_disabled", 0);
  await waitFor(
    () => !doc.querySelector(".description"),
    "After enabling via the pref, the panel's description is removed"
  );

  await checkTree(env, [
    {
      role: "document",
      name: `"TopLevel"`,
    },
  ]);

  await closeTabToolboxAccessibility(env.tab);
});

function checkTree(env, tree) {
  return runA11yPanelTests(
    [
      {
        expected: {
          tree,
        },
      },
    ],
    env
  );
}
