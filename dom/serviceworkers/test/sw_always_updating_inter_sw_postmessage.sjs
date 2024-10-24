/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// This file is derived from `self_update_worker.sjs`.
"use strict";

// We import the entirety of the normal, non-infinitely-updating SW rather than
// having meaningful script logic here.  This indirection doesn't really change
// anything.
const WORKER_BODY = `
console.log("Version", version, "importing helper");
importScripts("sw_inter_sw_postmessage.js");
console.log("Version", version, "imported helper");
`;

function handleRequest(request, response) {
  let count = getState("count");
  dump(`SJS: existing count is ${count}\n`);
  if (count === "") {
    count = 1;
  } else {
    count = parseInt(count);
    // test-verify mode unfortunately doesn't do anything on its own to reset
    // SJS state, which is unfortunate.  Our test only goes up to 5, so when we
    // hit 6 wrap back to 1.
    if (count === 6) {
      count = 1;
    }
  }
  dump(`SJS: using count of ${count}\n`);

  let worker = "var version = " + count + ";\n";
  worker = worker + WORKER_BODY;

  dump(`SJS BODY::::\n\n${worker}\n\n`);

  // This header is necessary for making this script able to be loaded.
  response.setHeader("Content-Type", "application/javascript");

  // If this is the first request, return the first source.
  response.write(worker);
  setState("count", "" + (count + 1));
}
