// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Test harness definitions from <https://github.com/tc39/test262/blob/main/harness/sm/non262.js>
// which aren't provided by our own test harness.

function createNewGlobal() {
  return $262.createRealm().global
}

if (typeof createExternalArrayBuffer === "undefined") {
  var createExternalArrayBuffer = size => new ArrayBuffer(size);
}

if (typeof enableGeckoProfilingWithSlowAssertions === "undefined") {
  var enableGeckoProfilingWithSlowAssertions = () => {};
}

if (typeof enableGeckoProfiling === "undefined") {
  var enableGeckoProfiling = () => {};
}

if (typeof disableGeckoProfiling === "undefined") {
  var disableGeckoProfiling = () => {};
}
