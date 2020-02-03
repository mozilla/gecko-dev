/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function Initialize(text) {
  try {
    return new Function(`${text} return exports`)();
  } catch (e) {
    dump(`Initialize Error: ${e}\n`);
  }
}

var EXPORTED_SYMBOLS = ["Initialize"];
