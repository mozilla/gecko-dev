/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const BASE_URL =
  "chrome://mochitests/content/browser/js/xpconnect/tests/browser/";
const WORKER_URL = BASE_URL + "worker_import_esm_source.mjs";

add_task(async function testInWorker() {
  const string = await new Promise(resolve => {
    const worker = new ChromeWorker(WORKER_URL, { type: "module" });
    worker.addEventListener("message", event => {
      resolve(event.data.string);
    });
  });
  ok(
    string.includes("A comment in function."),
    "Source should be available in ESM loaded via ChromeUtils.importESModule in worker"
  );
});
