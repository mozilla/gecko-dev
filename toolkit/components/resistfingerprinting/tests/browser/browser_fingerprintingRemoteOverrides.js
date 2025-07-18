/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

"use strict";

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

const COLLECTION_NAME = "fingerprinting-protection-overrides";

// The javascript bitwise operator only support 32bits. So, we only test
// RFPTargets that is under low 32 bits.
const TARGET_DEFAULT =
  Services.rfp.enabledFingerprintingProtections.getNth32BitSet(0);
const TARGET_PointerEvents = 1 << 2;
const TARGET_CanvasRandomization = 1 << 9;
const TARGET_WindowOuterSize = 1 << 26;
const TARGET_Gamepad = 1 << 24;

const TEST_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "empty.html";

const TEST_ANOTHER_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.net"
  ) + "empty.html";

const TEST_CASES = [
  // Test simple addition.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize",
        firstPartyDomain: "example.org",
      },
    ],
    expects: [
      {
        domain: "example.org",
        overrides: TARGET_DEFAULT | TARGET_WindowOuterSize,
      },
      { domain: "example.com", noEntry: true },
    ],
  },
  // Test simple subtraction
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "-WindowOuterSize",
        firstPartyDomain: "example.org",
      },
    ],
    expects: [
      {
        domain: "example.org",
        overrides: TARGET_DEFAULT & ~TARGET_WindowOuterSize,
      },
      { domain: "example.com", noEntry: true },
    ],
  },
  // Test simple subtraction for default targets.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "-CanvasRandomization",
        firstPartyDomain: "example.org",
      },
    ],
    expects: [
      {
        domain: "example.org",
        overrides: TARGET_DEFAULT & ~TARGET_CanvasRandomization,
      },
      { domain: "example.com", noEntry: true },
    ],
  },
  // Test multiple targets
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        firstPartyDomain: "example.org",
      },
    ],
    expects: [
      {
        domain: "example.org",
        overrides:
          (TARGET_DEFAULT | TARGET_WindowOuterSize | TARGET_PointerEvents) &
          ~TARGET_Gamepad,
      },
      { domain: "example.com", noEntry: true },
    ],
  },
  // Test multiple entries
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        firstPartyDomain: "example.org",
      },
      {
        id: "2",
        last_modified: 1000000000000001,
        overrides: "+Gamepad",
        firstPartyDomain: "example.com",
      },
    ],
    expects: [
      {
        domain: "example.org",
        overrides:
          (TARGET_DEFAULT | TARGET_WindowOuterSize | TARGET_PointerEvents) &
          ~TARGET_Gamepad,
      },
      {
        domain: "example.com",
        overrides: TARGET_DEFAULT | TARGET_Gamepad,
      },
    ],
  },
  // Test an entry with both first-party and third-party domains.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        firstPartyDomain: "example.org",
        thirdPartyDomain: "example.com",
      },
    ],
    expects: [
      {
        domain: "example.org,example.com",
        overrides:
          (TARGET_DEFAULT | TARGET_WindowOuterSize | TARGET_PointerEvents) &
          ~TARGET_Gamepad,
      },
    ],
  },
  // Test an entry with the first-party set to a wildcard.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        firstPartyDomain: "*",
      },
    ],
    expects: [
      {
        domain: "*",
        overrides:
          (TARGET_DEFAULT | TARGET_WindowOuterSize | TARGET_PointerEvents) &
          ~TARGET_Gamepad,
      },
    ],
  },
  // Test a entry with the first-party set to a wildcard and the third-party set
  // to a domain.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        firstPartyDomain: "*",
        thirdPartyDomain: "example.com",
      },
    ],
    expects: [
      {
        domain: "*,example.com",
        overrides:
          (TARGET_DEFAULT | TARGET_WindowOuterSize | TARGET_PointerEvents) &
          ~TARGET_Gamepad,
      },
    ],
  },
  // Test an entry with all targets disabled using '-AllTargets' but only enable one target.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "-AllTargets,+WindowOuterSize",
        firstPartyDomain: "*",
        thirdPartyDomain: "example.com",
      },
    ],
    expects: [
      {
        domain: "*,example.com",
        overrides: TARGET_WindowOuterSize,
      },
    ],
  },
  // Test an entry with all targets disabled using '-AllTargets' but enable some targets.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "-AllTargets,+WindowOuterSize,+Gamepad",
        firstPartyDomain: "*",
        thirdPartyDomain: "example.com",
      },
    ],
    expects: [
      {
        domain: "*,example.com",
        overrides: TARGET_WindowOuterSize | TARGET_Gamepad,
      },
    ],
  },
  // Test an invalid entry with only third party domain.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        thirdPartyDomain: "example.com",
      },
    ],
    expects: [
      {
        domain: "example.com",
        noEntry: true,
      },
    ],
  },
  // Test an invalid entry with both wildcards.
  {
    entires: [
      {
        id: "1",
        last_modified: 1000000000000001,
        overrides: "+WindowOuterSize,+PointerEvents,-Gamepad",
        firstPartyDomain: "*",
        thirdPartyDomain: "*",
      },
    ],
    expects: [
      {
        domain: "example.org",
        noEntry: true,
      },
    ],
  },
];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.fingerprintingProtection.remoteOverrides.testing", true]],
  });

  registerCleanupFunction(() => {
    Services.rfp.cleanAllOverrides();
  });
});

const client = RemoteSettings(COLLECTION_NAME);
const db = client.db;
async function addRemoteOverrides(entries) {
  const promise = promiseObserver("fpp-test:set-overrides-finishes");
  await client.db.clear();
  await client.db.importChanges({}, Date.now(), entries);

  await client.emit("sync", {});
  await promise;
}

add_task(async function test_remote_settings() {
  // Add initial empty record.
  await db.importChanges({}, Date.now(), []);

  for (let test of TEST_CASES) {
    info(`Testing with entry ${JSON.stringify(test.entires)}`);

    await addRemoteOverrides(test.entires);

    ok(true, "Got overrides update");

    for (let expect of test.expects) {
      // Get the addition and subtraction flags for the domain.
      try {
        let overrides = Services.rfp.getFingerprintingOverrides(
          expect.domain + ",0"
        );

        // Verify if the flags are matching to expected values.
        is(
          overrides.getNth32BitSet(0),
          expect.overrides,
          "The override value is correct."
        );
      } catch (e) {
        ok(expect.noEntry, "The override entry doesn't exist.");
      }
    }
  }

  db.clear();
});

add_task(async function test_remote_settings_pref() {
  // Add initial empty record.
  await db.importChanges({}, Date.now(), []);

  for (let test of TEST_CASES) {
    info(`Testing with entry ${JSON.stringify(test.entires)}`);

    // Disable remote overrides
    await SpecialPowers.pushPrefEnv({
      set: [
        ["privacy.fingerprintingProtection.remoteOverrides.enabled", false],
      ],
    });

    await addRemoteOverrides(test.entires);
    ok(true, "Got overrides update");

    for (let expect of test.expects) {
      try {
        // Check for the existance of RFP overrides
        Services.rfp.getFingerprintingOverrides(expect.domain + ",0");
        ok(
          false,
          "This line should never run as the override should not exist and the previous line would throw an exception"
        );
      } catch (e) {
        ok(true, "Received an exception as expected");
      }
    }
  }

  db.clear();
});

add_task(async function test_pref() {
  for (let test of TEST_CASES) {
    info(`Testing with entry ${JSON.stringify(test.entires)}`);

    // Create a promise for waiting the overrides get updated.
    let promise = promiseObserver("fpp-test:set-overrides-finishes");

    // Trigger the fingerprinting overrides update by setting the pref.
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "privacy.fingerprintingProtection.granularOverrides",
          JSON.stringify(test.entires),
        ],
      ],
    });

    await promise;
    ok(true, "Got overrides update");

    for (let expect of test.expects) {
      try {
        // Get the addition and subtraction flags for the domain.
        let overrides = Services.rfp.getFingerprintingOverrides(
          expect.domain + ",0"
        );

        // Verify if the flags are matching to expected values.
        is(
          overrides.getNth32BitSet(0),
          expect.overrides,
          "The override value is correct."
        );
      } catch (e) {
        ok(expect.noEntry, "The override entry doesn't exist.");
      }
    }
  }
});

// Verify if the pref overrides the remote settings.
add_task(async function test_pref_override_remote_settings() {
  // Add initial empty record.
  let db = RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, Date.now(), []);

  // Trigger a remote settings sync.
  await addRemoteOverrides([
    {
      id: "1",
      last_modified: 1000000000000001,
      overrides: "+WindowOuterSize",
      firstPartyDomain: "example.org",
    },
  ]);

  // Then, setting the pref.
  const promise = promiseObserver("fpp-test:set-overrides-finishes");
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "privacy.fingerprintingProtection.granularOverrides",
        JSON.stringify([
          {
            id: "1",
            last_modified: 1000000000000001,
            overrides: "+PointerEvents,-WindowOuterSize,-Gamepad",
            firstPartyDomain: "example.org",
          },
        ]),
      ],
    ],
  });
  await promise;

  // Get the addition and subtraction flags for the domain.
  let overrides = Services.rfp.getFingerprintingOverrides("example.org,0");

  // Verify if the flags are matching to the pref settings.
  is(
    overrides.getNth32BitSet(0),
    (TARGET_DEFAULT | TARGET_PointerEvents) &
      ~TARGET_Gamepad &
      ~TARGET_WindowOuterSize,
    "The override addition value is correct."
  );

  db.clear();
});

// Verify if the pref overrides the remote settings (again).
add_task(async function test_pref_override_remote_settings2() {
  // Add initial empty record.
  let db = RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, Date.now(), []);

  // Trigger a remote settings sync.
  await addRemoteOverrides([
    {
      id: "1",
      last_modified: 1000000000000001,
      overrides: "+PointerEvents,+Gamepad",
      firstPartyDomain: "example.org",
    },
  ]);

  // Then, setting the pref.
  const promise = promiseObserver("fpp-test:set-overrides-finishes");
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "privacy.fingerprintingProtection.granularOverrides",
        JSON.stringify([
          {
            id: "1",
            last_modified: 1000000000000001,
            overrides: "+WindowOuterSize",
            firstPartyDomain: "example.org",
          },
        ]),
      ],
    ],
  });
  await promise;

  // Get the addition and subtraction flags for the domain.
  let overrides = Services.rfp
    .getFingerprintingOverrides("example.org,0")
    .getNth32BitSet(0);

  // Verify if the flags are matching to the pref settings.
  is(
    overrides & TARGET_PointerEvents,
    0,
    "The override addition value should not have TARGET_PointerEvents."
  );

  is(
    overrides & TARGET_Gamepad,
    0,
    "The override addition value should not have TARGET_Gamepad."
  );

  is(
    overrides & TARGET_WindowOuterSize,
    TARGET_WindowOuterSize,
    "The override addition value should have TARGET_WindowOuterSize."
  );

  db.clear();
});

// Bug 1873682 - Verify that a third-party beacon request won't hit the
// assertion in nsRFPService::GetOverriddenFingerprintingSettingsForChannel().
add_task(async function test_beacon_request() {
  // Open an empty page.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_ANOTHER_PAGE],
    async url => {
      // Create a third-party iframe
      let ifr = content.document.createElement("iframe");

      await new content.Promise(resolve => {
        ifr.onload = resolve;
        content.document.body.appendChild(ifr);
        ifr.src = url;
      });

      await SpecialPowers.spawn(ifr, [url], url => {
        // Sending the beacon request right before the tab navigates away.
        content.addEventListener("unload", _ => {
          let value = ["text"];
          let blob = new Blob(value, {
            type: "application/x-www-form-urlencoded",
          });
          content.navigator.sendBeacon(url, blob);
        });
      });

      // Navigate the tab to another page.
      content.location = url;
    }
  );

  await BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    TEST_ANOTHER_PAGE
  );

  ok(true, "Successfully navigates away.");

  BrowserTestUtils.removeTab(tab);
});
