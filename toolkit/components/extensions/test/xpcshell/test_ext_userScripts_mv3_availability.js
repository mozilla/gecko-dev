"use strict";

// There are two implementations of a "userScripts" API in Firefox:
// - a legacy MV2-only API, tied to the user_scripts manifest key.
// - A cross-browser MV3 userScripts API, tied to the "userScripts" permission.
//
// This file verifies that these namespaces are fully isolated and limited to
// their respective manifest versions.

// This test expects and checks deprecation warnings.
ExtensionTestUtils.failOnSchemaWarnings(false);

// In release builds, content processes usually receive schemas that define
// content script APIs and not other APIs (i.e. scopes missing content_child).
// Although ext-toolkit.json registers user_scripts.json with scopes
// "addon_child", these schemas may be visible to content scripts when in debug
// builds because non-content schemas are always exposed when DEBUG is true:
// https://searchfox.org/mozilla-central/rev/26a98a7ba56f315df146512c43449412f0592942/toolkit/components/extensions/Schemas.sys.mjs#3864
//
// Because a Namespace definition can only have one "allowedContexts" value,
// and user_scripts_content.json specifies "allowedContexts": ["content"],
// all userScripts API definitions including the non-content definitions
// from user_scripts.json are loaded. When these definitions are loaded,
// the properties are exposed to content scripts (through lazy getters).
const ARE_NON_CONTENT_USER_SCRIPTS_APIS_EXPOSED_TO_CONTENT = AppConstants.DEBUG;

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/dummy", () => {});

// Test that manifest.user_scripts does not expose a userScripts API in MV3.
add_task(async function legacy_userScripts_unavailable_in_mv3() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      user_scripts: {},
    },
    background() {
      browser.test.assertEq(
        undefined,
        browser.userScripts,
        "Legacy userScripts API unavailable in MV3"
      );
      browser.test.sendMessage("bg_done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("bg_done");

  Assert.deepEqual(
    extension.extension.warnings,
    [
      `Reading manifest: Property "user_scripts" is unsupported in Manifest Version 3`,
    ],
    "Got expected warning when user_scripts manifest key is used in MV3"
  );

  await extension.unload();
});

// Test that manifest.user_scripts.api_script does not expose a userScripts API
// in content scripts.
add_task(async function legacy_userScripts_unavailable_in_mv3_content_script() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      user_scripts: {
        api_script: "api_script.js",
      },
      content_scripts: [
        {
          run_at: "document_end",
          js: ["content_script.js"],
          matches: ["*://example.com/dummy"],
        },
      ],
    },
    files: {
      "api_script.js": () => {
        browser.test.fail("Unexpected execution of api_script.js");
      },
      "content_script.js": function () {
        browser.test.assertEq(
          undefined,
          browser.userScripts,
          "Legacy userScripts API unavailable in MV3 content script"
        );
        browser.test.sendMessage("content_script_done");
      },
    },
  });
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );
  await extension.awaitMessage("content_script_done");
  await contentPage.close();

  Assert.deepEqual(
    extension.extension.warnings,
    [
      `Reading manifest: Property "user_scripts" is unsupported in Manifest Version 3`,
    ],
    "Got expected warning when user_scripts manifest key is used in MV3"
  );

  await extension.unload();
});

// Tests that when the legacy user_scripts key is present and the userScripts
// permission, that none of the MV3-specific functionality is exposed.
add_task(async function legacy_userScripts_plus_userScripts_permission_mv2() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 2,
      permissions: ["userScripts", "*://example.com/*"],
      user_scripts: {
        api_script: "api_script.js",
      },
      content_scripts: [
        {
          run_at: "document_end",
          js: ["content_script.js"],
          matches: ["*://example.com/dummy"],
        },
      ],
    },
    async background() {
      browser.test.assertDeepEq(
        ["UserScriptOptions", "onBeforeScript", "register"],
        Object.keys(browser.userScripts).sort(),
        "MV2 background script should only see legacy userScripts (not MV3) API"
      );

      // Due to a quirk of the Schemas internals, any property defined in the
      // schema has a lazy getter on the exported namespace object. Dereference
      // the lazy getter to trigger the full access check to reveal only the
      // APIs that are really available:
      Object.values(browser.userScripts);

      browser.test.assertDeepEq(
        ["register"],
        Object.keys(browser.userScripts).sort(),
        "Only the legacy userScripts.register method is available in MV2"
      );
      try {
        let retval = await browser.userScripts.register({
          js: [{ file: "userscript.js" }],
          runAt: "document_end",
          matches: ["*://example.com/dummy"],
        });
        browser.test.assertEq(
          "function",
          typeof retval.unregister,
          "Legacy register() should return object with unregister() method"
        );
        browser.test.assertThrows(
          () => browser.userScripts.register([]),
          "Incorrect argument types for userScripts.register.",
          "Expected error when MV3 userScripts API signature is called"
        );
      } catch (e) {
        browser.test.fail(`Unexpected error: ${e}`);
      }
      browser.test.sendMessage("bg_done");
    },
    files: {
      "api_script.js": () => {
        browser.userScripts.onBeforeScript.addListener(script => {
          script.defineGlobals({
            notifyUserScriptExecuted(typeofUserScripts) {
              browser.test.sendMessage("user_script_done", typeofUserScripts);
            },
          });
        });
        const userScriptsKeys = Object.keys(browser.userScripts).sort();
        browser.test.sendMessage("api_script_done", userScriptsKeys);
      },
      "userscript.js": () => {
        // exported by api_script.js
        // eslint-disable-next-line no-undef
        notifyUserScriptExecuted(typeof globalThis?.browser?.userScripts);
      },
      "content_script.js": function () {
        const userScriptsKeys = Object.keys(browser.userScripts).sort();
        browser.test.sendMessage("content_script_done", userScriptsKeys);
      },
    },
  });
  await extension.startup();
  await extension.awaitMessage("bg_done");

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );
  let [
    userScriptsKeysInApiScript,
    userScriptsKeysInContentScript,
    typeofUserScriptsInUserScript,
  ] = await Promise.all([
    extension.awaitMessage("api_script_done"),
    extension.awaitMessage("content_script_done"),
    extension.awaitMessage("user_script_done"),
  ]);
  await contentPage.close();

  const expectedUserScriptKeys = ["onBeforeScript"];
  if (ARE_NON_CONTENT_USER_SCRIPTS_APIS_EXPOSED_TO_CONTENT) {
    // The implementation defines these properties initially because until the
    // lazy getters are dereferenced, the implementation does not know that they
    // have no value.
    // Note that we intentionally check values instead of eagerly initializing
    // all values (e.g. through Object.values) because we want to verify that
    // there are no MV3-specific properties defined.
    expectedUserScriptKeys.push("UserScriptOptions");
    expectedUserScriptKeys.push("register");
    expectedUserScriptKeys.sort();
  }
  Assert.deepEqual(
    userScriptsKeysInApiScript,
    expectedUserScriptKeys,
    "Legacy api_script should only see legacy userScripts (not MV3) API"
  );
  Assert.deepEqual(
    userScriptsKeysInContentScript,
    expectedUserScriptKeys,
    "MV2 content script should only see legacy userScripts (not MV3) API"
  );
  Assert.equal(
    typeofUserScriptsInUserScript,
    "undefined",
    "Legacy user script does never have access to browser.userScripts"
  );

  Assert.deepEqual(
    extension.extension.warnings,
    [`Reading manifest: Permission "userScripts" requires Manifest Version 3.`],
    "Got expected warning when userScripts permission is used in MV2"
  );

  await extension.unload();
});

// Test that there are no traces of the legacy userScripts API in MV3, but only
// the new userScripts API in MV3.
add_task(async function legacy_userScripts_plus_userScripts_permission_mv3() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      permissions: ["userScripts"],
      host_permissions: ["*://example.com/*"],
      user_scripts: {
        api_script: "api_script.js",
      },
      content_scripts: [
        {
          run_at: "document_end",
          js: ["content_script.js"],
          matches: ["*://example.com/dummy"],
        },
      ],
    },
    async background() {
      browser.test.assertTrue(browser.userScripts, "userScripts API is in MV3");
      browser.test.assertEq(
        "function",
        typeof browser.userScripts.register,
        "userScripts.register should be defined in MV3"
      );
      browser.test.assertFalse(
        "onBeforeScript" in browser.userScripts,
        "Legacy userScripts.onBeforeScript is not exposed in MV3 extension"
      );
      browser.test.assertFalse(
        "UserScriptOptions" in browser.userScripts,
        "Legacy userScripts.UserScriptOptions is not exposed in MV3 extension"
      );

      // Schema quirk: The type is defined but resolves to nothing.
      browser.test.assertTrue(
        "RegisteredUserScript" in browser.userScripts,
        "New userScripts.RegisteredUserScript is exposed in MV3 extension"
      );
      browser.test.assertEq(
        undefined,
        browser.userScripts.RegisteredUserScript,
        "userScripts.RegisteredUserScript has no value"
      );

      browser.test.assertThrows(
        () => {
          browser.userScripts.register({
            js: [{ file: "userscript.js" }],
            runAt: "document_end",
            matches: ["*://example.com/dummy"],
          });
        },
        "Incorrect argument types for userScripts.register.",
        "Expected error when legacy userScripts API signature is called"
      );
      try {
        let retval = await browser.userScripts.register([]);
        browser.test.assertEq(
          undefined,
          retval,
          "userScripts.register() should NOT return an object from legacy API"
        );
      } catch (e) {
        browser.test.fail(`Unexpected error: ${e}`);
      }
      browser.test.sendMessage("bg_done");
    },
    files: {
      "api_script.js": () => {
        browser.test.fail("Unexpected execution of api_script.js");
      },
      "userscript.js": () => {
        // User scripts cannot access extension APIs, so browser.test.fail()
        // cannot be called here. We just throw (which would result in a log
        // message instead of a hard failure). The implementation runs
        // api_script.js before userscript.js, so any unexpected execution is
        // expected to be caught by the api_script.js check.
        throw new Error("Unexpected execution of userscript.js");
      },
      "content_script.js": function () {
        let userScriptsKeys = browser.userScripts;
        if (browser.userScripts) {
          // When ARE_NON_CONTENT_USER_SCRIPTS_APIS_EXPOSED_TO_CONTENT is true,
          // the userScripts namespace is defined with many lazy properties.
          // We need to dereference the lazy getters before we can confirm that
          // the namespace is empty.
          Object.values(browser.userScripts);
          userScriptsKeys = Object.keys(browser.userScripts);
        }
        browser.test.sendMessage("content_script_done", userScriptsKeys);
      },
    },
  });
  await extension.startup();
  await extension.awaitMessage("bg_done");

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );
  let userScriptsKeys = await extension.awaitMessage("content_script_done");
  if (ARE_NON_CONTENT_USER_SCRIPTS_APIS_EXPOSED_TO_CONTENT) {
    // Because the userScripts namespace is declared to have some content APIs
    // (even if MV2-only), the whole namespace is sent to the content process
    // when ARE_NON_CONTENT_USER_SCRIPTS_APIS_EXPOSED_TO_CONTENT is true. This
    // does usually not happen to users, but we can encounter it in tests.
    // The "browser.userScripts" namespace ends up being an empty object because
    // "allowedContexts": ["content"] is not specified on any of the userScripts
    // API members (except onBeforeScript, but that has max_manifest_version:2).
    Assert.deepEqual(
      userScriptsKeys,
      [],
      "MV3 content script should not see a userScripts namespace, or at most an empty object"
    );
  } else {
    Assert.deepEqual(
      userScriptsKeys,
      undefined,
      "MV3 content script should not see a userScripts namespace"
    );
  }
  await contentPage.close();

  Assert.deepEqual(
    extension.extension.warnings,
    [
      `Reading manifest: Property "user_scripts" is unsupported in Manifest Version 3`,
    ],
    "Got expected warning when user_scripts manifest key is used in MV3"
  );

  await extension.unload();
});
