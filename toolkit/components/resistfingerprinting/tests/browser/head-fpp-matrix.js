/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const DOMAIN = "example.com";
const EMPTY_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://" + DOMAIN
  ) + "empty.html";

const FPP_PREF = "privacy.fingerprintingProtection";
const ENABLE_FPP = [FPP_PREF, true];
const DISABLE_FPP = [FPP_PREF, false];

const FPP_PBM_PREF = "privacy.fingerprintingProtection.pbmode";
const ENABLE_FPP_PBM = [FPP_PBM_PREF, true];
const DISABLE_FPP_PBM = [FPP_PBM_PREF, false];

const RFP_TARGET = "NetworkConnection";
const OVERRIDES_ENABLED = `-AllTargets,+${RFP_TARGET}`;
const OVERRIDES_DISABLED = "-AllTargets";
const GRANULAR_OVERRIDES_ENABLED = JSON.stringify([
  {
    firstPartyDomain: DOMAIN,
    overrides: OVERRIDES_ENABLED,
  },
]);
const GRANULAR_OVERRIDES_DISABLED = JSON.stringify([
  {
    firstPartyDomain: DOMAIN,
    overrides: OVERRIDES_DISABLED,
  },
]);

const FPP_OVERRIDES_ENABLED = [
  "privacy.fingerprintingProtection.overrides",
  OVERRIDES_ENABLED,
];
const FPP_OVERRIDES_DISABLED = [
  "privacy.fingerprintingProtection.overrides",
  OVERRIDES_DISABLED,
];
const FPP_GRANULAR_OVERRIDES_ENABLED = [
  "privacy.fingerprintingProtection.granularOverrides",
  GRANULAR_OVERRIDES_ENABLED,
];
const FPP_GRANULAR_OVERRIDES_DISABLED = [
  "privacy.fingerprintingProtection.granularOverrides",
  GRANULAR_OVERRIDES_DISABLED,
];

const BFPP_PREF = "privacy.baselineFingerprintingProtection";
const ENABLE_BFPP = [BFPP_PREF, true];
const DISABLE_BFPP = [BFPP_PREF, false];

const BFPP_OVERRIDES_ENABLED = [
  "privacy.baselineFingerprintingProtection.overrides",
  OVERRIDES_ENABLED,
];
const BFPP_OVERRIDES_DISABLED = [
  "privacy.baselineFingerprintingProtection.overrides",
  OVERRIDES_DISABLED,
];
const BFPP_GRANULAR_OVERRIDES_ENABLED = [
  "privacy.baselineFingerprintingProtection.granularOverrides",
  GRANULAR_OVERRIDES_ENABLED,
];
const BFPP_GRANULAR_OVERRIDES_DISABLED = [
  "privacy.baselineFingerprintingProtection.granularOverrides",
  GRANULAR_OVERRIDES_DISABLED,
];

function generateTestCases(bfppIsGranular, fppIsGranular) {
  const result = [];
  const len = 5;
  const vals = [true, false];
  const combinations = Array.from(
    { length: Math.pow(vals.length, len) },
    (_, index) => {
      return Array.from(
        { length: len },
        (_, i) =>
          vals[
            Math.floor(index / Math.pow(vals.length, len - 1 - i)) % vals.length
          ]
      );
    }
  );
  const [
    BFPP_ENABLED_I,
    FPP_ENABLED_I,
    FPP_PBM_ENABLED_I,
    BFPP_OVERRIDES_ENABLED_I,
    FPP_OVERRIDES_ENABLED_I,
  ] = [...Array(len).keys()];
  const truthfulnessToPref = [
    [ENABLE_BFPP, DISABLE_BFPP],
    [ENABLE_FPP, DISABLE_FPP],
    [ENABLE_FPP_PBM, DISABLE_FPP_PBM],
    bfppIsGranular
      ? [BFPP_GRANULAR_OVERRIDES_ENABLED, BFPP_GRANULAR_OVERRIDES_DISABLED]
      : [BFPP_OVERRIDES_ENABLED, BFPP_OVERRIDES_DISABLED],
    fppIsGranular
      ? [FPP_GRANULAR_OVERRIDES_ENABLED, FPP_GRANULAR_OVERRIDES_DISABLED]
      : [FPP_OVERRIDES_ENABLED, FPP_OVERRIDES_DISABLED],
  ];
  const computeExpectedResults = combination => {
    let expectedNormalBrowsing = false;
    let expectedPrivateBrowsing = false;

    // If FPP is enabled, it overrides everything.
    if (combination[FPP_ENABLED_I]) {
      expectedNormalBrowsing = combination[FPP_OVERRIDES_ENABLED_I];
      expectedPrivateBrowsing = combination[FPP_OVERRIDES_ENABLED_I];
      return { expectedNormalBrowsing, expectedPrivateBrowsing };
    }

    // If FPP_PBM is enabled, it overrides bFPP in private browsing.
    if (combination[FPP_PBM_ENABLED_I]) {
      expectedPrivateBrowsing = combination[FPP_OVERRIDES_ENABLED_I];
      if (combination[BFPP_ENABLED_I]) {
        expectedNormalBrowsing = combination[BFPP_OVERRIDES_ENABLED_I];
      } else {
        expectedNormalBrowsing = false;
      }
      return { expectedNormalBrowsing, expectedPrivateBrowsing };
    }

    // Both FPP and FPP_PBM are disabled.
    if (combination[BFPP_ENABLED_I]) {
      expectedNormalBrowsing = combination[BFPP_OVERRIDES_ENABLED_I];
      expectedPrivateBrowsing = combination[BFPP_OVERRIDES_ENABLED_I];
      return { expectedNormalBrowsing, expectedPrivateBrowsing };
    }

    return { expectedNormalBrowsing, expectedPrivateBrowsing };
  };
  for (const combination of combinations) {
    const prefs = combination.map(
      (val, i) => truthfulnessToPref[i][val ? 0 : 1]
    );
    const expectedVals = computeExpectedResults(combination);
    result.push({
      description: `BFPP: ${combination[BFPP_ENABLED_I]}, FPP: ${combination[FPP_ENABLED_I]}, FPP_PBM: ${combination[FPP_PBM_ENABLED_I]}, BFPP_OVERRIDES: ${combination[BFPP_OVERRIDES_ENABLED_I]}, FPP_OVERRIDES: ${combination[FPP_OVERRIDES_ENABLED_I]}, bfppIsGranular: ${bfppIsGranular}, fppIsGranular: ${fppIsGranular}`,
      expectedNormalBrowsing: expectedVals.expectedNormalBrowsing,
      expectedPrivateBrowsing: expectedVals.expectedPrivateBrowsing,
      prefs,
    });
  }
  return result;
}

async function runTestCase(
  index,
  testCase,
  normalBrowsingWindow,
  privateBrowsingWindow
) {
  await SpecialPowers.pushPrefEnv({
    set: testCase.prefs,
  });

  for (const isPBM of [false, true]) {
    const win = isPBM ? privateBrowsingWindow : normalBrowsingWindow;
    const expectedValue = isPBM
      ? testCase.expectedPrivateBrowsing
      : testCase.expectedNormalBrowsing;
    const description = `${index}: In ${isPBM ? "private browsing" : "normal browsing"}. ${testCase.description}`;

    const tab = await BrowserTestUtils.openNewForegroundTab(
      win.gBrowser,
      EMPTY_PAGE
    );

    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [expectedValue, description],
      async function (expectedValue, description) {
        ok(
          "connection" in content.navigator,
          "navigator.connection should exist"
        );

        const result = content.navigator.connection.type === "unknown";
        is(result, expectedValue, description);
      }
    );

    BrowserTestUtils.removeTab(tab);
  }

  await SpecialPowers.popPrefEnv();
}

let normalBrowsingWindow;
let privateBrowsingWindow;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.netinfo.enabled", true]],
  });

  normalBrowsingWindow = await BrowserTestUtils.openNewBrowserWindow();
  privateBrowsingWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(normalBrowsingWindow);
    await BrowserTestUtils.closeWindow(privateBrowsingWindow);

    await SpecialPowers.popPrefEnv();
  });
});

async function runTestCases(testCases) {
  for (let i = 0; i < testCases.length; i++) {
    await runTestCase(
      i,
      testCases[i],
      normalBrowsingWindow,
      privateBrowsingWindow
    );
  }
}
