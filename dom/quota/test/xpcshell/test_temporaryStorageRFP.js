/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

async function testSteps() {
  const principal = getPrincipal("http://example.com");
  const GiB = 1024 * 1024 * 1024;

  // Set the limit to some random value that is less than 50 GiB.
  const globalLimitBytes = 1 * GiB;
  const globalLimitKib = globalLimitBytes / 1024;

  setGlobalLimit(globalLimitKib);

  let request = init();
  await requestFinished(request);

  request = initTemporaryStorage();
  await requestFinished(request);

  request = estimateOrigin(principal);
  await requestFinished(request);

  const perGroupPercentage = 0.2;
  const expectedGroupLimitBytes = Math.floor(
    globalLimitBytes * perGroupPercentage
  );
  is(expectedGroupLimitBytes, request.result.limit);

  // Verify the RFP override is applied.
  request = reset();
  await requestFinished(request);

  let spoofedLimitBytes = 50 * GiB;
  if (AppConstants.platform == "android") {
    spoofedLimitBytes = 32 * GiB;
  }

  Services.prefs.setBoolPref("privacy.resistFingerprinting", true);

  request = init();
  await requestFinished(request);

  request = initTemporaryStorage();
  await requestFinished(request);

  request = estimateOrigin(principal);
  await requestFinished(request);

  const expectedSpoofedGroupLimitBytes = Math.floor(
    spoofedLimitBytes * perGroupPercentage
  );
  is(
    expectedSpoofedGroupLimitBytes,
    request.result.limit,
    "RFP limit should be applied"
  );

  Services.prefs.clearUserPref("privacy.resistFingerprinting");

  resetGlobalLimit();

  request = reset();
  await requestFinished(request);
}
