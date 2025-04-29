/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

addAccessibleTask(
  `
<button id="btn1" accesskey="s">foo</button>
<div id="btn2" role="button" aria-keyshortcuts="Alt+Shift+f">bar</div>

  `,
  async function () {
    is(
      await runPython(`
      global doc
      doc = getDocIa2()
      btn = findIa2ByDomId(doc, "btn1")
      return btn.accKeyboardShortcut(CHILDID_SELF)
    `),
      "Alt+Shift+s",
      "btn1 has correct keyboard shortcut"
    );

    is(
      await runPython(`
      btn = findIa2ByDomId(doc, "btn2")
      return btn.accKeyboardShortcut(CHILDID_SELF)
    `),
      "Alt+Shift+f",
      "btn2 has correct keyboard shortcut"
    );
  },
  { chrome: true, topLevel: true }
);
