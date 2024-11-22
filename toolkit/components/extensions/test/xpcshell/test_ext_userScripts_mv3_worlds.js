"use strict";

const server = createHttpServer({ hosts: ["example.com", "example.net"] });
server.registerPathHandler("/dummy", () => {});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

const { ExtensionPermissions } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

async function grantUserScriptsPermission(extensionId) {
  // userScripts is optional-only, and we must grant it. See comment at
  // grantUserScriptsPermission in test_ext_userScripts_mv3_availability.js.
  await ExtensionPermissions.add(extensionId, {
    permissions: ["userScripts"],
    origins: [],
  });
}

async function spawnPage(spawnFunc) {
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );
  const results = await contentPage.spawn([], spawnFunc);
  await contentPage.close();
  return results;
}

add_setup(async () => {
  Services.prefs.setBoolPref("extensions.userScripts.mv3.enabled", true);
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function default_USER_SCRIPT_world_behavior() {
  const extensionId = "@default_USER_SCRIPT_world_behavior";
  await grantUserScriptsPermission(extensionId);
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: extensionId } },
      manifest_version: 3,
      optional_permissions: ["userScripts"],
      host_permissions: ["*://example.com/*"],
    },
    files: {
      "world_checker.js": () => {
        window.wrappedJSObject.results = [];

        // window.eval is not blocked and runs in MAIN world.
        // eslint-disable-next-line no-eval
        let resultsInMainWorld = window.eval("results");

        // Unlike ISOLATED, USER_SCRIPT world cannot access extension APIs.
        resultsInMainWorld.push(typeof browser === "undefined");

        // Unlike MAIN, USER_SCRIPT world's default CSP blocks eval.
        try {
          // eslint-disable-next-line no-eval
          eval("throw new Error('eval executed unexpectedly???')");
        } catch (e) {
          resultsInMainWorld.push(e.message);
        }
      },
    },
    async background() {
      await browser.userScripts.register([
        {
          id: "world global checker",
          matches: ["*://example.com/dummy"],
          js: [{ file: "world_checker.js" }],
          runAt: "document_end",
          world: "USER_SCRIPT",
        },
      ]);
      browser.test.sendMessage("registered");
    },
  });

  await extension.startup();
  await extension.awaitMessage("registered");

  const results = await spawnPage(() => this.content.wrappedJSObject.results);
  equal(results[0], true, "browser (extension APIs) should be undefined");
  equal(
    results[1],
    "call to eval() blocked by CSP",
    "eval() should be blocked by default in the USER_SCRIPT world"
  );

  await extension.unload();
});

add_task(async function multiple_scripts_share_same_default_world() {
  const extensionId = "@multiple_scripts_share_same_default_world";
  await grantUserScriptsPermission(extensionId);
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: extensionId } },
      manifest_version: 3,
      optional_permissions: ["userScripts"],
      host_permissions: ["*://example.com/*"],
    },
    async background() {
      await browser.userScripts.register([
        {
          id: "first scripts",
          matches: ["*://example.com/dummy"],
          js: [
            // All js within one script are guaranteed to run in order, and
            // they should run in the same default sandbox.
            { code: `var a = "";` },
            { code: `a += "1";` },
            { code: `a += "2";` },
            { code: `a += "3";` },
            { code: `a += "4";` },
            { code: `a += "5";` },
            // Prepends a to x, and expose in web page as variable "r".
            { code: `var x = x || ""; x = a + x; window.wrappedJSObject.r=x` },
          ],
          runAt: "document_end",
        },
        {
          id: "separate scripts",
          matches: ["*://example.com/dummy"],
          js: [
            { code: `var b = "";` },
            { code: `b += "6";` },
            { code: `b += "7";` },
            { code: `b += "8";` },
            { code: `b += "9";` },
            // Appends a to x, and expose in web page as variable "r".
            { code: `var x = x || ""; x = x + b; window.wrappedJSObject.r=x` },
          ],
          runAt: "document_end",
        },
        {
          id: "document_start, runs before other document_end scripts",
          matches: ["*://example.com/dummy"],
          // The other scripts prepend and append, and this becomes the middle.
          js: [{ code: `var x = x || "_"; window.wrappedJSObject.r=x` }],
          runAt: "document_start",
        },
      ]);
      browser.test.sendMessage("registered");
    },
  });

  await extension.startup();
  await extension.awaitMessage("registered");

  const result = await spawnPage(() => {
    let { x, r } = this.content.wrappedJSObject;
    return { x, r };
  });
  equal(result.x, undefined, "Web page cannot see vars from USER_SCRIPT world");
  equal(result.r, "12345_6789", "All user scripts should share the same scope");

  await extension.unload();
});

add_task(async function test_worldId_validation() {
  const extensionId = "@test_worldId_validation";
  await grantUserScriptsPermission(extensionId);
  async function background() {
    const id = "single user script id";
    function testRegister(props) {
      // ^ Not async, so that callers can test the difference between sync vs
      // async errors from userScripts.register().
      return browser.userScripts.register([
        { id, includeGlobs: ["*"], js: [{ code: "// ..." }], ...props },
      ]);
    }
    async function doUnregister() {
      await browser.userScripts.unregister({ ids: [id] });
    }

    try {
      await browser.test.assertRejects(
        testRegister({ worldId: "_" }),
        "Invalid worldId: _",
        "worldId starting with underscore are reserved"
      );
      browser.test.log("worldId containing underscore after start is OK");
      await testRegister({ worldId: "x_" });
      await doUnregister();

      await browser.test.assertRejects(
        testRegister({ worldId: "x".repeat(257) }),
        /^Invalid worldId: x{257}$/,
        "Too long worldId is rejected"
      );
      browser.test.log("worldId length of 256 characters is OK");
      await testRegister({ worldId: "x".repeat(256) });
      await doUnregister();
      browser.test.log("worldId length of 256 double-byte characters is OK");
      await testRegister({ worldId: "\u{1234}".repeat(256) });
      await doUnregister();
      // The above shows that we do not count by the number of bytes, but by
      // the JS string length. The following assertion shows that we do not
      // somehow count by the number of Unicode characters.
      await browser.test.assertRejects(
        testRegister({ worldId: "\u{1f00d}".repeat(256) }),
        /^Invalid worldId: \u{1f00d}{256}$/u,
        "worldId length of 256 multi-code unit characters is rejected."
      );

      browser.test.assertThrows(
        () => testRegister({ worldId: 123 }),
        /worldId: Expected string instead of 123/,
        "Non-string worldId is rejected."
      );

      // Now test that worldId cannot be used with world "MAIN".
      await browser.test.assertRejects(
        testRegister({ world: "MAIN", worldId: "i" }),
        "worldId cannot be used with MAIN world.",
        "Should not support worldId with MAIN world"
      );

      await browser.test.assertRejects(
        testRegister({ world: "MAIN", worldId: "i" }),
        "worldId cannot be used with MAIN world.",
        "Should not support worldId with MAIN world"
      );

      // And not even with update().
      await testRegister({ world: "MAIN" });
      await browser.test.assertRejects(
        browser.userScripts.update([{ id, worldId: "y" }]),
        "worldId cannot be used with MAIN world.",
        "Should not update worldId to non-default world for world MAIN"
      );
      browser.test.log("Updating worldId + world=USER_SCRIPT at once is OK");
      await browser.userScripts.update([
        { id, world: "USER_SCRIPT", worldId: "y" },
      ]);
      await browser.test.assertRejects(
        browser.userScripts.update([{ id, world: "MAIN" }]),
        "worldId cannot be used with MAIN world.",
        "Should not change world to MAIN when worldId is non-default worldId"
      );
      browser.test.log("Update can set world=MAIN and clear worldId at once");
      await browser.userScripts.update([{ id, world: "MAIN", worldId: "" }]);
      await doUnregister();
    } catch (e) {
      browser.test.fail(`Unexpected error: ${e}`);
    }
    browser.test.sendMessage("done");
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: { gecko: { id: extensionId } },
      manifest_version: 3,
      optional_permissions: ["userScripts"],
    },
    background,
  });
  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});

add_task(async function test_default_and_many_non_default_worldIds() {
  const extensionId = "@test_default_and_many_non_default_worldIds";
  await grantUserScriptsPermission(extensionId);
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: extensionId } },
      manifest_version: 3,
      optional_permissions: ["userScripts"],
      host_permissions: ["*://example.com/*"],
    },
    async background() {
      const matches = ["*://example.com/dummy"];
      let scripts = [
        {
          id: "define res, so other scripts can push results",
          matches,
          js: [{ code: `window.res = [];` }],
          runAt: "document_start",
          world: "MAIN",
          // worldId not specified - not meaningful for world "MAIN".
        },
        {
          id: "default world (worldId not specified)",
          matches,
          js: [{ code: `var x = x ?? []; x.push(-3)` }],
          runAt: "document_start",
          world: "USER_SCRIPT",
          // worldId not specified = default world.
        },
        {
          id: "default world (worldId is empty string)",
          matches,
          js: [{ code: `var x = x ?? []; x.push(-2)` }],
          runAt: "document_start",
          world: "USER_SCRIPT",
          worldId: "", // worldId "" is the default world.
        },
        {
          id: "default world (worldId is null)",
          matches,
          js: [{ code: `var x = x ?? []; x.push(-1)` }],
          runAt: "document_start",
          world: "USER_SCRIPT",
          worldId: null, // worldId null defaults to default world.
        },
        {
          id: "default world (export result from previous scripts)",
          matches,
          js: [{ code: `window.wrappedJSObject.res.push(...x)` }],
          runAt: "document_end", // Runs after document_start in default world.
          world: "USER_SCRIPT",
          // worldId not specified = default world.
        },
      ];
      // expected result is [-3, -2, -1] is from default world above.
      const expectedResults = [-3, -2, -1]; // plus 1...50 from loop below.
      for (let i = 1; i <= 50; ++i) {
        expectedResults.push(i);
        // The first script initializes "x" if not done so before, the second
        // script exports it to the main world.
        scripts.push({
          id: `user script ${i} at document_start`,
          matches,
          js: [{ code: `var x = x ?? ${i};` }],
          runAt: "document_start",
          world: "USER_SCRIPT",
          worldId: `worldId ${i}`,
        });
        scripts.push({
          id: `user script ${i} at document_end`,
          matches,
          // If worlds were unexpectedly shared, x would be from another script
          // and false would be added to res instead of the number i.
          js: [{ code: `window.wrappedJSObject.res.push(x === ${i} && ${i})` }],
          runAt: "document_end",
          world: "USER_SCRIPT",
          worldId: `worldId ${i}`,
        });
      }
      await browser.userScripts.register(scripts);
      browser.test.sendMessage("registered_and_expected", expectedResults);
    },
  });

  await extension.startup();
  let expectedRes = await extension.awaitMessage("registered_and_expected");

  const actualRes = await spawnPage(() => this.content.wrappedJSObject.res);
  // Script execution order is not guaranteed yet between different scripts,
  // so print informative message:
  info(`Actual result (unsorted): ${actualRes}`);

  Assert.deepEqual(
    actualRes.toSorted((a, b) => a - b),
    expectedRes,
    "Every script should execute in the world specified by worldId"
  );

  await extension.unload();
});
