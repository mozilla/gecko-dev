/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function () {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsole.open", true);
  const dbg = await initDebugger(
    "doc-script-switching.html",
    "script-switching-01.js"
  );

  await selectSource(dbg, "script-switching-01.js");

  info("Open the split console");
  await getDebuggerSplitConsole(dbg);
  ok(dbg.toolbox.splitConsole, "Split console is shown.");

  info("Click to focus on the debugger editor");
  await clickElement(dbg, "codeMirror");

  info("Press ESC to close the split console");
  pressKey(dbg, "Escape");
  ok(!dbg.toolbox.splitConsole, "Split console is hidden.");

  info("Press the ESC again to open the split console again");
  pressKey(dbg, "Escape");
  ok(dbg.toolbox.splitConsole, "Split console is shown.");
});
