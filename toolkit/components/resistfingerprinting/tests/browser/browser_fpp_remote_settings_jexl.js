/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

ChromeUtils.defineESModuleGetters(this, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

const DOMAIN = "example.com";
const DOMAIN_KEY = DOMAIN + ",0";

const TEST_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://" + DOMAIN
  ) + "empty.html";

const COLLECTION_NAME = "fingerprinting-protection-overrides";

const TARGET_PointerEvents = 1 << 2;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.fingerprintingProtection.remoteOverrides.testing", true]],
  });

  registerCleanupFunction(() => {
    Services.rfp.cleanAllOverrides();
  });
});

const client = RemoteSettings(COLLECTION_NAME);

async function addRemoteOverride(filterExp) {
  const promise = promiseObserver("fpp-test:set-overrides-finishes");
  await client.db.clear();
  await client.db.create({
    firstPartyDomain: DOMAIN,
    overrides: "-AllTargets,+PointerEvents",
    filter_expression: filterExp,
  });
  await client.db.importChanges({}, Date.now());

  await client.emit("sync", {});
  await promise;
}

add_task(async function () {
  await addRemoteOverride(`'128.0.1'|versionCompare('127.0a1') >= 0`);

  let overrides = null;
  try {
    overrides = Services.rfp
      .getFingerprintingOverrides(DOMAIN_KEY)
      .getNth32BitSet(0);
    ok(
      true,
      "getFingerprintingOverrides should succeed because there should be overrides"
    );
  } catch {
    ok(
      false,
      "getFingerprintingOverrides should not fail because there should be overrides"
    );
  }

  is(
    overrides,
    TARGET_PointerEvents,
    "The overrides are set properly when current version is greater than minimum required version"
  );

  await addRemoteOverride(`'127.0.1'|versionCompare('128.0a1') >= 0`);

  try {
    overrides = Services.rfp
      .getFingerprintingOverrides(DOMAIN_KEY)
      .getNth32BitSet(0);
    ok(
      false,
      "getFingerprintingOverrides should not succeed because there should be no overrides"
    );
  } catch {
    ok(
      true,
      "getFingerprintingOverrides should fail because there should be no overrides"
    );
  }
});
