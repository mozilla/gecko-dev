/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

{
  const HEADER_TAG = "ipprotection-header";
  const CONTENT_TAG = "ipprotection-content";

  for (let [tag, script] of [
    [
      HEADER_TAG,
      "chrome://browser/content/ipprotection/ipprotection-header.mjs",
    ],
    [
      CONTENT_TAG,
      "chrome://browser/content/ipprotection/ipprotection-content.mjs",
    ],
  ]) {
    if (!customElements.get(tag)) {
      customElements.setElementCreationCallback(tag, () => {
        ChromeUtils.importESModule(script, { global: "current" });
      });
    }
  }
}
