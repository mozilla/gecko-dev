/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests opening the console first, clicking a link
// opens the editor at the correct location.

"use strict";

add_task(async function () {
  const toolbox = await initPane("doc-script-switching.html", "webconsole");
  const linkEl = await waitForConsoleMessageLink(
    toolbox,
    "hi",
    "script-switching-02.js:18:9"
  );
  linkEl.click();

  await waitFor(() => toolbox.getPanel("jsdebugger"));
  const dbg = createDebuggerContext(toolbox);
  await waitForElement(dbg, "highlightLine");
  assertHighlightLocation(dbg, "script-switching-02.js", 18);
  assertCursorPosition(dbg, 18, 9, "Cursor is set at the right location");
});
