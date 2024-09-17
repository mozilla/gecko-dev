/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* global getJSTestingFunctions */

/**
 * Bug 1889762 - Test FPP granualar overrides for service workers
 */

// browser_service_worker_overrides.js already tests the timezone offset override
// from privacy.fingerprintingProtection.overrides, so we know that it works.
// To test timezone offset *granular* overrides, we just need to disable it
// granularly and check that the override is not applied.

const getTimezoneOffset = async win => {
  return await runFunctionInServiceWorker(win, async () => {
    await getJSTestingFunctions().setTimeZone("PST8PDT");
    const date = new Date();
    const offset = date.getTimezoneOffset();
    await getJSTestingFunctions().setTimeZone(undefined);
    return offset;
  });
};

const createMessage = (scope, isFirstParty, expected) =>
  `For scope ${scope}, timezone offset should be ${expected} in the service worker in the ${
    isFirstParty ? "first-party" : "third-party"
  } context`;

const runTest = async (browser, expected, scope, isFirstParty) => {
  const timeZoneOffset = await getTimezoneOffset(browser);

  info("Got: " + timeZoneOffset);
  is(timeZoneOffset, expected, createMessage(scope, isFirstParty, expected));
};

const basePrefs = [
  ["privacy.fingerprintingProtection", true],
  ["privacy.fingerprintingProtection.overrides", "+JSDateTimeUTC"],
  ["privacy.resistFingerprinting", false],
  ["dom.serviceWorkers.testing.enabled", true],
];

// Iterating over the scopes in https://searchfox.org/mozilla-central/rev/261005fcc4d6f8b64189946958211259fb45e9e1/toolkit/components/resistfingerprinting/nsRFPService.cpp#2247

// Test { * } granular override.
// If the key is *, the override will be applied to all contexts.
add_task(async () => {
  const scope = "{ * }";
  await firstAndThirdPartyContextRunner(
    async browser => runTest(browser, 420, scope, true),
    async browser => runTest(browser, 420, scope, false),
    [
      ...basePrefs,
      [
        "privacy.fingerprintingProtection.granularOverrides",
        JSON.stringify([
          {
            firstPartyDomain: "*",
            overrides: "-JSDateTimeUTC",
          },
        ]),
      ],
    ]
  );
});

// Test { firstPartyDomain, * } granular override.
// Every context that is under the given first-party domain, including itself.
add_task(async () => {
  const scope = "{ firstPartyDomain, * }";
  await firstAndThirdPartyContextRunner(
    async browser => runTest(browser, 420, scope, true),
    async browser => runTest(browser, 420, scope, false),
    [
      ...basePrefs,
      [
        "privacy.fingerprintingProtection.granularOverrides",
        JSON.stringify([
          {
            firstPartyDomain: "example.com",
            thirdPartyDomain: "*",
            overrides: "-JSDateTimeUTC",
          },
        ]),
      ],
    ]
  );
});

// Test { firstPartyDomain } granular override.
// First-party contexts that load the given first-party domain.
add_task(async () => {
  const scope = "{ firstPartyDomain }";
  await firstAndThirdPartyContextRunner(
    async browser => runTest(browser, 420, scope, true),
    async browser => runTest(browser, 0, scope, false),
    [
      ...basePrefs,
      [
        "privacy.fingerprintingProtection.granularOverrides",
        JSON.stringify([
          {
            firstPartyDomain: "example.com",
            overrides: "-JSDateTimeUTC",
          },
        ]),
      ],
    ]
  );
});

// Test { *, thirdPartyDomain } granular override.
// Every third-party context that loads the given third-party domain.
add_task(async () => {
  const scope = "{ *, third-party domain }";
  await firstAndThirdPartyContextRunner(
    async browser => runTest(browser, 0, scope, true),
    async browser => runTest(browser, 420, scope, false),
    [
      ...basePrefs,
      [
        "privacy.fingerprintingProtection.granularOverrides",
        JSON.stringify([
          {
            firstPartyDomain: "*",
            thirdPartyDomain: "example.net",
            overrides: "-JSDateTimeUTC",
          },
        ]),
      ],
    ]
  );
});

// Test { firstPartyDomain, thirdPartyDomain } granular override.
// The third-party context that is under the given first-party domain.
add_task(async () => {
  const scope = "{ firstPartyDomain, thirdPartyDomain }";
  await firstAndThirdPartyContextRunner(
    async browser => runTest(browser, 0, scope, true),
    async browser => runTest(browser, 420, scope, false),
    [
      ...basePrefs,
      [
        "privacy.fingerprintingProtection.granularOverrides",
        JSON.stringify([
          {
            firstPartyDomain: "example.com",
            thirdPartyDomain: "example.net",
            overrides: "-JSDateTimeUTC",
          },
        ]),
      ],
    ]
  );
});

const firstPartyPageURL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "empty.html";

const thirdPartyPageURL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.net"
  ) + "empty.html";

const firstAndThirdPartyContextRunner = async (
  firstPartyContextTest,
  thirdPartyContextTest,
  prefs
) => {
  // Set the prefs.
  await SpecialPowers.pushPrefEnv({
    set: prefs,
  });

  // Create a page to run the tests in.
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    firstPartyPageURL
  );
  const firstPartyBrowser = tab.linkedBrowser;

  // Create a 3rd party content in the first party context.
  const thirdPartyBrowser = await SpecialPowers.spawn(
    firstPartyBrowser,
    [thirdPartyPageURL],
    async url => {
      const w = content;
      const iframe = w.document.createElement("iframe");

      const { promise: frameLoaded, resolve } = Promise.withResolvers();
      iframe.onload = resolve;
      iframe.src = url;
      w.document.body.appendChild(iframe);
      await frameLoaded;

      return iframe.browsingContext;
    }
  );

  // Run the tests.
  await firstPartyContextTest(firstPartyBrowser);
  await thirdPartyContextTest(thirdPartyBrowser);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
};
