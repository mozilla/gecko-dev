/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * This tests that MAR update network requests from a non-Firefox
 * application DO NOT include extras -- headers and query parameters
 * -- identifying whether the request comes from a browsing profile or
 * a background task.
 */

// See Bug 1929581.
const TOOLKIT_ISSUE_FIXED = false;

const downloadHeaders = new DownloadHeadersTest();

add_setup(async () => {
  setupTestCommon(true, /* aAllowBits */ false);
  downloadHeaders.startUpdateServer();
});

// Outside of Firefox, no headers are expected.
const expectedExtras = [
  { mode: null, name: null },
  { mode: null, name: null },
];

const canRun = {
  skip_if: () => !TOOLKIT_ISSUE_FIXED,
};

add_task(canRun, async function test_noTask_noBITS() {
  await downloadHeaders.test({
    useBits: false,
    backgroundTaskName: null,
    userAgentPattern: /\sGecko\//,
    expectedExtras,
  });
});

const canTask = {
  skip_if: () => !AppConstants.MOZ_BACKGROUNDTASKS,
};

add_task(canTask, async function test_task_noBITS() {
  await downloadHeaders.test({
    useBits: false,
    backgroundTaskName: "task_downloadHeaders",
    userAgentPattern: /\sGecko\//,
    expectedExtras,
  });
});

const canBits = {
  skip_if: () => {
    // Need to enable the pref to accurately test for BITS.
    Services.prefs.setBoolPref(PREF_APP_UPDATE_BITS_ENABLED, true);
    return !gAUS.canUsuallyUseBits;
  },
};

add_task(canBits, async function test_noTask_BITS() {
  await downloadHeaders.test({
    useBits: true,
    backgroundTaskName: null,
    userAgentPattern: /^Microsoft BITS\//,
    expectedExtras,
  });
});

const canTaskBits = {
  skip_if: () => {
    // Need to enable the pref to accurately test for BITS.
    Services.prefs.setBoolPref(PREF_APP_UPDATE_BITS_ENABLED, true);
    return !AppConstants.MOZ_BACKGROUNDTASKS || !gAUS.canUsuallyUseBits;
  },
};

add_task(canTaskBits, async function test_task_BITS() {
  await downloadHeaders.test({
    useBits: true,
    backgroundTaskName: "task_downloadHeaders",
    userAgentPattern: /^Microsoft BITS\//,
    expectedExtras,
  });
});

add_task(async () => {
  doTestFinish();
});
