/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "handleDevToolsPacket", () => {
  return ChromeUtils.importESModule(
    "resource://devtools/server/startup/worker.sys.mjs",
    { global: "current" }
  ).handleDevToolsPacket;
});

this.addEventListener("message", async function (event) {
  const packet = JSON.parse(event.data);
  // Switch on packet type to target different protocols: DevTools or BiDi
  lazy.handleDevToolsPacket(packet);
});
