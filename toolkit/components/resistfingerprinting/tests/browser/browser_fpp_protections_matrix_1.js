/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* import-globals-from head-fpp-matrix.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/resistfingerprinting/tests/browser/head-fpp-matrix.js",
  this
);

requestLongerTimeout(4);

add_task(async function verifyOverridesNonGranular() {
  await SpecialPowers.pushPrefEnv({
    set: [["test.wait300msAfterTabSwitch", true]],
  });

  await runTestCases(generateTestCases(false, false));
});
