"use strict";

const { ExtensionPermissions } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

const { ExtensionTestCommon } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionTestCommon.sys.mjs"
);

const { ExtensionUserScripts } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionUserScripts.sys.mjs"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

function loadTestExtension({ background }) {
  const id = Services.uuid.generateUUID().number;
  // "userScripts" is an optional-only permission, so we need to grant it.
  // ExtensionPermissions.add() is async, but not waiting here is OK for us:
  // Extension startup is blocked on reading permissions, which in turn awaits
  // any previous ExtensionPermissions calls in FIFO order.
  ExtensionPermissions.add(id, { permissions: ["userScripts"], origins: [] });
  return ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id } },
      manifest_version: 3,
      optional_permissions: ["userScripts"],
    },
    background,
    files: {
      "file1.js": "",
      "file2.js": "",
    },
  });
}

add_setup(async () => {
  Services.prefs.setBoolPref("extensions.userScripts.mv3.enabled", true);
  await ExtensionTestUtils.startAddonManager();
});

let _contentPage;
// Helper to verify that user script registrations are propagated as expected.
async function lookupScriptsInChild(extensionId, keys) {
  if (!_contentPage) {
    // We just want a content process, don't really care where it is.
    _contentPage = await ExtensionTestUtils.loadContentPage("about:blank");
    registerCleanupFunction(() => _contentPage.close());
  }
  return _contentPage.spawn([extensionId, keys], (extensionId, keys) => {
    const { WebExtensionPolicy, MatchPatternSet } = this.content;

    // User scripts are only considered for scheduling when they have been
    // registered as a WebExtensionContentScript with WebExtensionPolicy.
    // We summarize their values for the test assertions, here.
    let policy = WebExtensionPolicy.getByID(extensionId);
    return policy?.contentScripts.map(script => {
      let out = {};
      for (let k of keys) {
        let v = script[k];
        if (MatchPatternSet.isInstance(v)) {
          v = v.patterns.map(p => p.pattern);
        }
        out[k] = v;
      }
      return out;
    });
  });
}

// Test the documented flow: register user scripts at runtime.onInstalled, and
// expect them to still be registered after a restart. Verifies that the
// registration is both visible through the API and in the content process.
add_task(async function register_and_restart() {
  let extension = loadTestExtension({
    background() {
      const scriptIn = {
        id: "test1",
        matches: ["https://example.com/*"],
        js: [{ file: "file1.js" }, { file: "file2.js" }],
      };
      const expectedScriptOut = {
        ...scriptIn,
        allFrames: false,
        excludeMatches: null,
        includeGlobs: null,
        excludeGlobs: null,
        runAt: "document_idle",
        // Notable call-out: All other content script APIs default to "ISOLATED"
        // as the default world. In the userScripts API, we want to default to
        // "USER_SCRIPT" when the world is not specified.
        world: "USER_SCRIPT",
        worldId: "",
      };
      browser.runtime.onInstalled.addListener(async ({ reason }) => {
        browser.test.assertEq("install", reason, "onInstalled reason");
        await browser.userScripts.register([scriptIn]);
        browser.test.assertDeepEq(
          [expectedScriptOut],
          await browser.userScripts.getScripts(),
          "getScripts() after register"
        );
        browser.test.sendMessage("onInstalled");
      });
      browser.test.onMessage.addListener(async msg => {
        browser.test.assertEq("testStillRegistered", msg, "Expected message");
        browser.test.assertDeepEq(
          [expectedScriptOut],
          await browser.userScripts.getScripts(),
          "getScripts() still finds scripts"
        );
        browser.test.sendMessage("testStillRegistered:done");
      });
    },
  });

  await extension.startup();
  await extension.awaitMessage("onInstalled");

  const extensionId = extension.id;
  const expectedScriptsInContent = [
    {
      matches: ["https://example.com/*"],
      jsPaths: [
        `moz-extension://${extension.uuid}/file1.js`,
        `moz-extension://${extension.uuid}/file2.js`,
      ],
    },
  ];
  Assert.deepEqual(
    await lookupScriptsInChild(extensionId, ["matches", "jsPaths"]),
    expectedScriptsInContent,
    "Expected WebExtensionContentScript instances after registration"
  );

  // Simulate browser restart. While promiseShutdownManager unloads the add-on,
  // it is independent of the internal database used by ExtensionUserScripts.
  // To verify that the data is really stored on disk, wipe the in-memory state
  // through _uninitForTesting() so that when the extension starts up again,
  // that the userScripts state is read from disk again.
  await AddonTestUtils.promiseShutdownManager();
  ExtensionUserScripts._getStoreForTesting()._uninitForTesting();
  await AddonTestUtils.promiseStartupManager();

  Assert.deepEqual(
    await lookupScriptsInChild(extensionId, ["matches", "jsPaths"]),
    expectedScriptsInContent,
    "Expected WebExtensionContentScript instances after app+extension startup"
  );

  info("Waking up event page so that test.onMessage can receive a message");
  await extension.wakeupBackground();
  info("Verifying that the scripts are still registered after browser restart");
  extension.sendMessage("testStillRegistered");
  await extension.awaitMessage("testStillRegistered:done");
  await extension.unload();

  Assert.deepEqual(
    await lookupScriptsInChild(extensionId, ["matches", "jsPaths"]),
    null,
    "No user scripts after extension unload"
  );
});

// Test that the userScripts.update() API can be called and update some or all
// properties, while leaving null values untouched. Also confirms that a fully
// populated registration is correctly propagated to the content process.
// Moreover, verifies that code can be persisted and is persisted the internal
// blob:-URL is refreshed after a browser restart.
add_task(async function register_and_update_all_values() {
  let extension = loadTestExtension({
    background() {
      const id = "userScript_id";
      const scriptDefaults = {
        id,
        allFrames: false,
        js: [],
        // matches or includeGlobs must be non-empty, we cannot use [] here.
        matches: ["https://example.com/*"],
        excludeMatches: [],
        includeGlobs: [],
        excludeGlobs: [],
        runAt: "document_idle",
        world: "USER_SCRIPT",
        worldId: "",
      };
      const scriptDefaultsOut = {
        ...scriptDefaults,
        // An input of [] is equivalent to omitted, which is returned as null.
        // Chrome does the same.
        excludeMatches: null,
        includeGlobs: null,
        excludeGlobs: null,
      };
      // Keep this object in sync with expectedScriptsInContent below.
      const scriptNonDefaults = {
        id,
        allFrames: true,
        js: [
          { code: "// Hello world!" },
          { file: "file1.js" },
          { code: "//".repeat(1000) },
        ],
        matches: ["https://example.org/path/*"],
        excludeMatches: ["*://*/excludeme"],
        includeGlobs: ["*"],
        excludeGlobs: ["*excludeme*"],
        runAt: "document_end",
        world: "MAIN",
        // We cannot test a non-default worldId, because worldId can only take
        // a non-empty string with world USER_SCRIPT. Test coverage for worldId
        // is in test_ext_userScripts_mv3_worlds.js, test_worldId_validation.
        worldId: "",
      };

      const nullUpdate = Object.fromEntries(
        Object.keys(scriptDefaults).map(k => [k, null])
      );

      async function testRegisterDefaults() {
        await browser.userScripts.register([scriptDefaults]);
        browser.test.assertDeepEq(
          [scriptDefaultsOut],
          await browser.userScripts.getScripts(),
          "getScripts() after register with near-default properties"
        );

        await browser.userScripts.update([{ id }]);
        browser.test.assertDeepEq(
          [scriptDefaultsOut],
          await browser.userScripts.getScripts(),
          "userScripts.update call without update should not change anything"
        );

        await browser.userScripts.update([{ ...nullUpdate, id }]);
        browser.test.assertDeepEq(
          [scriptDefaultsOut],
          await browser.userScripts.getScripts(),
          "userScripts.update with all fields null should not change anything"
        );

        await browser.userScripts.update([scriptNonDefaults]);
        browser.test.assertDeepEq(
          [scriptNonDefaults],
          await browser.userScripts.getScripts(),
          "userScripts.update can change every property to a non-default value"
        );

        await browser.userScripts.update([scriptDefaults]);
        browser.test.assertDeepEq(
          [scriptDefaultsOut],
          await browser.userScripts.getScripts(),
          "userScripts.update restores non-default values when not null"
        );
      }

      async function testRegisterNonDefaults() {
        await browser.userScripts.register([scriptNonDefaults]);
        browser.test.assertDeepEq(
          [scriptNonDefaults],
          await browser.userScripts.getScripts(),
          "getScripts() after register with non-default properties"
        );

        await browser.userScripts.update([{ id }]);
        browser.test.assertDeepEq(
          [scriptNonDefaults],
          await browser.userScripts.getScripts(),
          "userScripts.update call without update should not change anything"
        );

        await browser.userScripts.update([{ ...nullUpdate, id }]);
        browser.test.assertDeepEq(
          [scriptNonDefaults],
          await browser.userScripts.getScripts(),
          "userScripts.update with all fields null should not change anything"
        );

        await browser.userScripts.update([scriptDefaults]);
        browser.test.assertDeepEq(
          [scriptDefaultsOut],
          await browser.userScripts.getScripts(),
          "userScripts.update can change every property to a near-default value"
        );

        // Sanity check that we can register the original js/code again:
        await browser.userScripts.update([scriptNonDefaults]);
        browser.test.assertDeepEq(
          [scriptNonDefaults],
          await browser.userScripts.getScripts(),
          "userScripts.update can restore original non-default values"
        );
      }

      browser.runtime.onInstalled.addListener(async () => {
        await testRegisterDefaults();
        await browser.userScripts.unregister({ ids: [id] });
        await testRegisterNonDefaults();
        browser.test.sendMessage("start_done");
      });
    },
  });
  await extension.startup();
  await extension.awaitMessage("start_done");

  async function checkScriptsInContentProcess(previousJsPaths = null) {
    const expectedScriptsInContent = {
      isUserScript: true,
      allFrames: true,
      matches: ["https://example.org/path/*"],
      excludeMatches: ["*://*/excludeme"],
      // Cannot inspect includeGlobs & excludeGlobs because they are not
      // exposed in WebExtensionContentScript.webidl
      // includeGlobs: ["*"],
      // excludeGlobs: ["*excludeme*"],
      runAt: "document_end",
      world: "MAIN",
      worldId: "",
    };
    const expectedScriptKeys = Object.keys(expectedScriptsInContent);
    Assert.deepEqual(
      await lookupScriptsInChild(extension.id, expectedScriptKeys),
      [expectedScriptsInContent],
      "Got initial script registration"
    );
    let [{ jsPaths }] = await lookupScriptsInChild(extension.id, ["jsPaths"]);
    equal(jsPaths.length, 3, "test case had 3 script registrations");
    ok(
      jsPaths[0].startsWith("blob:null"),
      `js with 'code' key (short string) becomes a blob:-URL: ${jsPaths[0]}`
    );
    equal(
      jsPaths[1],
      `moz-extension://${extension.uuid}/file1.js`,
      "js with 'file' key becomes a moz-extension:-URL"
    );
    ok(
      jsPaths[2].startsWith("blob:null"),
      `js with 'code' key (long string) becomes a blob:-URL: ${jsPaths[0]}`
    );
    if (previousJsPaths) {
      // When registered again, the blob:-URLs change.
      Assert.notEqual(jsPaths[0], previousJsPaths[0], "blob: changed (short)");
      Assert.notEqual(jsPaths[2], previousJsPaths[2], "blob: changed (long)");
    }
    return jsPaths;
  }

  const previousJsPaths = await checkScriptsInContentProcess(null);

  await AddonTestUtils.promiseShutdownManager();
  ExtensionUserScripts._getStoreForTesting()._uninitForTesting();
  await AddonTestUtils.promiseStartupManager();

  await extension.awaitStartup();

  // The background script should be stopped since it is an event page and we
  // are not triggering any persistent listeners that would wake it.
  ExtensionTestCommon.testAssertions.assertBackgroundStatusStopped(extension);

  const currentJsPaths = await checkScriptsInContentProcess(previousJsPaths);

  const blobUrls = currentJsPaths.filter(url => url.startsWith("blob:"));
  for (const blobUrl of blobUrls) {
    info(`Confirming that the URLs are resolved`);
    let res = await fetch(blobUrl);
    let txt = await res.text();
    // We are just doing a prefix check here as a sanity check. The main thing
    // we care about is that the URL resolves.
    equal(txt.slice(0, 2), "//", `blob:-url ${blobUrl} maps to expected code`);
  }

  await extension.unload();

  for (const blobUrl of blobUrls) {
    equal(URL.isValidObjectURL(blobUrl), false, `Revoked URL: ${blobUrl}`);
    if (AppConstants.platform == "android") {
      // On Android, unlike desktop, revoked blob:-URLs can still be loaded by
      // the system principal: https://searchfox.org/mozilla-central/rev/fcf53e1685bfb990b5abc7312ac1daf617f0991f/dom/file/uri/BlobURLChannel.cpp#62-69
      // The rationale for that logic is at https://bugzilla.mozilla.org/show_bug.cgi?id=1432949
      // The URL will eventually be revoked (after 5 seconds), but we are not
      // going to pause the test for 5 seconds, and instead rely on the
      // isValidObjectURL check above.
      continue;
    }
    await Assert.rejects(
      fetch(blobUrl),
      /NetworkError/,
      `blob:-URL should be revoked after extension uninstall: ${blobUrl}`
    );
  }
});

add_task(async function no_partial_registrations_on_error() {
  async function background() {
    try {
      const scriptBase = {
        matches: ["https://example.com/*"],
        js: [{ code: "// original" }, { file: "file1.js" }],
      };
      // For extra coverage, verify that we can call these APIs without
      // awaiting the promise. They are not expected to interfere with each
      // other.
      let promiseDuplicateSameCall = browser.userScripts.register([
        { ...scriptBase, id: "valid:ignore_me_1" },
        { ...scriptBase, id: "duplic" },
        { ...scriptBase, id: "duplic" },
        { ...scriptBase, id: "valid:ignore_me_2" },
      ]);
      let promiseReservedId = browser.userScripts.register([
        { ...scriptBase, id: "valid:ignore_me_3" },
        { ...scriptBase, id: "_reserved" },
        { ...scriptBase, id: "valid:ignore_me_4" },
      ]);
      let promiseValidBefore = browser.userScripts.register([
        { ...scriptBase, id: "validBefore1" },
        { ...scriptBase, id: "validBefore2" },
      ]);
      let promiseDuplicateBefore = browser.userScripts.register([
        { ...scriptBase, id: "validBefore1" }, // duplicate
      ]);
      let promiseUpdateNonExistent = browser.userScripts.update([
        { id: "validBefore1", js: [{ code: "// ignore me 1" }] },
        { ...scriptBase, id: "non_existent" },
      ]);
      let promiseUpdateDuplicate = browser.userScripts.update([
        { id: "validBefore1", js: [{ code: "// ignore me 2" }] },
        { id: "validBefore2", js: [{ code: "// ignore me 3" }] },
        { id: "validBefore2", js: [{ code: "// ignore me 4" }] },
      ]);
      let promiseUpdateNoMatchesNorGlobs = browser.userScripts.update([
        { id: "validBefore1", js: [{ code: "// ignore me 5" }] },
        { id: "validBefore2", matches: [] }, // = matches + includeGlobs empty
      ]);

      await browser.test.assertRejects(
        promiseDuplicateSameCall,
        "Duplicate script id: duplic",
        "userScripts.register() should reject two scripts with same id"
      );
      await browser.test.assertRejects(
        promiseReservedId,
        "Invalid id for RegisteredUserScript.",
        "userScripts.register() should reject script with reserved ID"
      );

      browser.test.assertEq(
        await promiseValidBefore,
        undefined,
        "userScripts.register() succeeded (validBefore1 + validBefore2)"
      );
      await browser.test.assertRejects(
        promiseDuplicateBefore,
        `User script with id "validBefore1" is already registered.`,
        "userScripts.register() should fail for existing ID"
      );
      await browser.test.assertRejects(
        promiseUpdateNonExistent,
        `User script with id "non_existent" does not exist.`,
        "userScripts.update() should fail for non-existing ID"
      );
      await browser.test.assertRejects(
        promiseUpdateDuplicate,
        "Duplicate script id: validBefore2",
        "userScripts.update() should reject two scripts with same id"
      );
      await browser.test.assertRejects(
        promiseUpdateNoMatchesNorGlobs,
        "matches or includeGlobs must be specified.",
        "userScripts.update() should reject update resulting in no matches"
      );

      const summarize = ({ id, matches, js }) => ({ id, matches, js });
      browser.test.assertDeepEq(
        [
          { ...scriptBase, id: "validBefore1" },
          { ...scriptBase, id: "validBefore2" },
        ],
        (await browser.userScripts.getScripts()).map(summarize),
        "When failures happen, unrelated scripts should not register."
      );

      // To complement promiseUpdateNoMatchesNorGlobs, verify that we can
      // indeed set to an empty matches, when includeGlobs is set.
      await browser.userScripts.update([
        { id: "validBefore2", matches: [], includeGlobs: ["ht*"] },
      ]);
      browser.test.assertDeepEq(
        [
          {
            matches: null,
            js: scriptBase.js,
            id: "validBefore2",
            includeGlobs: ["ht*"],
          },
        ],
        Array.from(
          await browser.userScripts.getScripts({ ids: ["validBefore2"] }),
          s => ({ ...summarize(s), includeGlobs: s.includeGlobs })
        ),
        "userScripts.update() with empty matches is OK if includeGlobs is set."
      );

      await browser.userScripts.unregister({
        ids: ["not_existing_start", "validBefore2", "not_existing_end"],
      });

      browser.test.assertDeepEq(
        [{ ...scriptBase, id: "validBefore1" }],
        (await browser.userScripts.getScripts()).map(summarize),
        "unregister() with non-existing IDs is not an error."
      );
    } catch (e) {
      browser.test.fail(`Unexpected failure: ${e}`);
    }
    browser.test.sendMessage("done");
  }
  let extension = loadTestExtension({ background });
  await extension.startup();
  await extension.awaitMessage("done");

  // Now also look at the internals to verify proper cleanup.
  let { userScriptsManager, registeredContentScripts } =
    WebExtensionPolicy.getByID(extension.id).extension;
  Assert.deepEqual(
    Array.from(userScriptsManager.scriptIdsMap.keys()),
    ["validBefore1"],
    "userScriptsManager.scriptIdsMap should contain only one entry"
  );

  equal(userScriptsManager.blobUrls.size, 1, "Should only have one blob URL");

  equal(registeredContentScripts.size, 1, "Should only have 1 internal script");

  Assert.deepEqual(
    await ExtensionUserScripts._getStoreForTesting().getAllEntries(),
    [
      [
        `${extension.id}/_script_/validBefore1`,
        {
          // In some cases below, the database contains null, because we store
          // the entry from userScripts.register(), which does not fill in the
          // default value through the schema because the same type is shared
          // with userScripts.update(), and we don't want to automatically set
          // values to the default upon update.
          // The publicly observable behavior (userScripts.getScripts() and
          // script execution behavior uses the correct defaults).
          id: "validBefore1",
          allFrames: null, // false,
          js: [{ code: "// original" }, { file: "file1.js" }],
          matches: ["https://example.com/*"],
          excludeMatches: null,
          includeGlobs: null,
          excludeGlobs: null,
          runAt: null, // "document_idle",
          world: null, // "USER_SCRIPT",
          worldId: null, // ""
        },
      ],
    ],
    "The database should only contain one entry, no junk"
  );

  await extension.unload();
});
