/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test previewing variables with the same name in different frames (Bug 1954182)

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-preview-multiple-frames.html");

  invokeInTab("fn1");
  await waitForPaused(dbg);

  info("Preview obj in the top frame");
  await assertPreviews(dbg, [
    {
      line: 6,
      column: 9,
      header: "Object",
      fields: [["prop", "true"]],
      expression: "obj",
    },
  ]);

  info("Preview obj in the second frame");
  clickElement(dbg, "frame", 2);
  await waitForSelectedLocation(dbg, 3);
  await assertPreviews(dbg, [
    {
      line: 2,
      column: 9,
      header: "Object",
      fields: [["prop", "false"]],
      expression: "obj",
    },
  ]);
});
