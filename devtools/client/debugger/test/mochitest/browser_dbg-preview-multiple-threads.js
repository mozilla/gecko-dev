/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test previewing variables with the same name in different threads (Bug 1954182)

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-preview-multiple-threads.html");

  invokeInTab("fn1");
  await waitForPaused(dbg);
  info("Preview data in the first worker");
  await assertPreviews(dbg, [
    {
      line: 1,
      column: 16,
      header: "Object",
      fields: [["prop", "true"]],
      expression: "data",
    },
  ]);

  // find the currently paused thread
  const allThreads = [...findAllElements(dbg, "threadsPaneItems")];
  const firstWorkerThreadIndex = allThreads.findIndex(thread =>
    thread.classList.contains("paused")
  );

  info("Preview data in the second worker");
  invokeInTab("fn2");
  await waitForSelectedLocation(dbg, 5);
  await assertPreviews(dbg, [
    {
      line: 1,
      column: 16,
      header: "Object",
      fields: [["prop", "false"]],
      expression: "data",
    },
  ]);

  info("Switch back to the first worker thread and preview data again");
  clickElement(dbg, "threadsPaneItem", firstWorkerThreadIndex + 1);
  await waitForSelectedLocation(dbg, 3);
  await assertPreviews(dbg, [
    {
      line: 1,
      column: 16,
      header: "Object",
      fields: [["prop", "true"]],
      expression: "data",
    },
  ]);
});
