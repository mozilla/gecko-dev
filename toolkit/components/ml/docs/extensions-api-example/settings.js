/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

function deleteCachedModels(event) {
  // Prevent the form from submitting
  event.preventDefault();

  // eslint-disable-next-line no-undef
  browser.trial.ml.deleteCachedModels().then(_res => {
    alert("Files deleted");
  });
}

document.querySelector("form").addEventListener("submit", deleteCachedModels);
