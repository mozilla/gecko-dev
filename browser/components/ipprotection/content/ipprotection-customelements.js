/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

{
  const TAG = "ipprotection-panel";

  if (!customElements.get(TAG)) {
    customElements.setElementCreationCallback(TAG, () => {
      ChromeUtils.importESModule(
        "chrome://browser/content/ipprotection/ipprotection-panel.mjs",
        { global: "current" }
      );
    });
  }
}
