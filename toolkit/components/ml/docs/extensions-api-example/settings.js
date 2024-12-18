/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* globals browser */

function deleteCachedModels() {
  browser.trial.ml.deleteCachedModels().then(_res => {
    alert("Files deleted");
  });
}

async function askPermission() {
  await browser.permissions.request({ permissions: ["trialML"] });
  await updateGranted();
}

async function updateGranted() {
  let granted = await browser.permissions.contains({
    permissions: ["trialML"],
  });
  document.body.classList.toggle("granted", granted);
}

document.querySelector("#grant").addEventListener("click", askPermission);
document.querySelector("#clear").addEventListener("click", deleteCachedModels);

updateGranted();
