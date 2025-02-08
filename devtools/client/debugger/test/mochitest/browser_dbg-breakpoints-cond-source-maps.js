/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Confirms that a conditional panel is opened at the
// correct location in generated files.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-sourcemaps.html", "entry.js");

  await selectSource(dbg, "bundle.js");
  await scrollEditorIntoView(dbg, 55, 0);

  await setLogPoint(dbg, 55);
  ok(
    !!(await getConditionalPanelAtLine(dbg, 55)),
    "conditional panel panel is open on line 55"
  );
  is(
    dbg.selectors.getConditionalPanelLocation().line,
    55,
    "conditional panel location is line 55"
  );
});
