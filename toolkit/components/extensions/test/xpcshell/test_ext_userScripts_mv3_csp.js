"use strict";

const { ExtensionUserScripts } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionUserScripts.sys.mjs"
);

const server = createHttpServer({ hosts: ["example.com", "example.net"] });
server.registerPathHandler("/evalChecker", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(`<script>var res = { evalOk: [], evalBlocked: [] };</script>`);
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

add_setup(async () => {
  Services.prefs.setBoolPref("extensions.userScripts.mv3.enabled", true);

  // Grant "userScripts" permission via permissions.request() without UI.
  Services.prefs.setBoolPref(
    "extensions.webextOptionalPermissionPrompts",
    false
  );

  await ExtensionTestUtils.startAddonManager();
});

async function testEvalCheckerWithUserScripts() {
  // Open page. User script(s) should run there.
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/evalChecker"
  );
  const res = await contentPage.spawn([], () => content.wrappedJSObject.res);
  await contentPage.close();
  return {
    // We do not want to rely on a particular execution order, so sort results.
    evalOk: res.evalOk.toSorted(),
    evalBlocked: res.evalBlocked.toSorted(),
  };
}

async function startEvalTesterExtension() {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      manifest_version: 3,
      optional_permissions: ["userScripts"],
      host_permissions: ["*://example.com/*"],
    },
    background() {
      // This function will be serialized and executed as a user script.
      function testEvalInWorld(worldId) {
        try {
          // eslint-disable-next-line no-eval
          eval("// If blocked by CSP, then this will throw");
          window.wrappedJSObject.res.evalOk.push(worldId);
        } catch {
          window.wrappedJSObject.res.evalBlocked.push(worldId);
        }
      }
      browser.test.onMessage.addListener(async (msg, args) => {
        if (msg === "grantUserScriptsPermission") {
          await browser.permissions.request({ permissions: ["userScripts"] });
        } else if (msg === "registerUserScriptForWorldId") {
          const worldId = args;
          await browser.userScripts.register([
            {
              id: `script for world ${worldId}.`,
              matches: ["*://example.com/evalChecker"],
              js: [{ code: `(${testEvalInWorld})("${worldId}")` }],
              runAt: "document_end",
              worldId: worldId,
            },
          ]);
        } else if (msg === "configureWorld") {
          await browser.userScripts.configureWorld(args);
        } else if (msg === "resetWorldConfiguration") {
          await browser.userScripts.resetWorldConfiguration(args);
        } else if (msg === "getWorldConfigurations") {
          let res = await browser.userScripts.getWorldConfigurations();
          for (let properties of res) {
            // We are only interested in the worldId / csp properties, so drop
            // all other properties so we can keep the assertions simple.
            delete properties.messaging;
          }
          browser.test.sendMessage("getWorldConfigurations:done", res);
          return;
        } else {
          browser.test.fail(`Unexpected message: ${msg}`);
        }
        browser.test.sendMessage(`${msg}:done`);
      });
    },
  });

  await extension.startup();

  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("grantUserScriptsPermission");
    await extension.awaitMessage("grantUserScriptsPermission:done");
  });

  async function queryExtension(msg, args) {
    info(`queryExtension(${msg}, ${args && JSON.stringify(args)})`);
    extension.sendMessage(msg, args);
    return extension.awaitMessage(`${msg}:done`);
  }
  return { extension, queryExtension };
}

// The default behavior is to enforce a strict CSP that blocks eval(), as shown
// by default_USER_SCRIPT_world_behavior in test_ext_userScripts_mv3_worlds.js.
// This test shows the most common scenario for user script extensions: that
// they can relax the CSP.
add_task(async function test_clear_csp_in_default_USER_SCRIPT_world() {
  let { extension, queryExtension } = await startEvalTesterExtension();

  await queryExtension("configureWorld", { csp: "" });
  await queryExtension("registerUserScriptForWorldId", "");
  await queryExtension("registerUserScriptForWorldId", "otherWorld");

  Assert.deepEqual(
    await testEvalCheckerWithUserScripts(),
    { evalOk: ["", "otherWorld"], evalBlocked: [] },
    "With empty csp in configureWorld(), user scripts can call eval()."
  );

  Assert.deepEqual(
    await queryExtension("getWorldConfigurations"),
    [{ worldId: "", csp: "" }],
    "getWorldConfigurations() returns current CSP configuration"
  );

  await extension.unload();
});

// Tests configureWorld, resetWorldConfiguration and getWorldConfigurations for
// multiple worldId (including default world):
// - verify that world-specific CSP can be set.
// - verify that if csp is null, that we fall back to the default world's CSP,
//   or the extension's (strict) default CSP if there is no default configured.
add_task(async function test_world_specific_csp_override() {
  let { extension, queryExtension } = await startEvalTesterExtension();

  // Override default: instead of blocking eval(), permit eval():
  await queryExtension("configureWorld", { worldId: "world_1", csp: "" });
  await queryExtension("configureWorld", {
    csp: "script-src 'none'", // Stricter than the default, and blocks eval().
    worldId: "world_3",
  });
  await queryExtension("registerUserScriptForWorldId", "");
  await queryExtension("registerUserScriptForWorldId", "world_1");
  await queryExtension("registerUserScriptForWorldId", "world_2");
  await queryExtension("registerUserScriptForWorldId", "world_3");

  Assert.deepEqual(
    await testEvalCheckerWithUserScripts(),
    { evalOk: ["world_1"], evalBlocked: ["", "world_2", "world_3"] },
    "Can override CSP for specific world without affecting default world"
  );

  info("Setting empty CSP by default, to allow eval().");
  await queryExtension("configureWorld", { worldId: "", csp: "" });
  Assert.deepEqual(
    await testEvalCheckerWithUserScripts(),
    { evalOk: ["", "world_1", "world_2"], evalBlocked: ["world_3"] },
    "CSP empty by default, except world_3 that blocked it explicitly"
  );

  info("Resetting CSP of default world");
  await queryExtension("resetWorldConfiguration", null);
  Assert.deepEqual(
    await testEvalCheckerWithUserScripts(),
    { evalOk: ["world_1"], evalBlocked: ["", "world_2", "world_3"] },
    "Default (strict) CSP is applied by default, except where allowed (world_1)"
  );

  // Sanity check: Which worlds do we have configured?
  Assert.deepEqual(
    await queryExtension("getWorldConfigurations"),
    [
      { worldId: "world_1", csp: "" },
      { worldId: "world_3", csp: "script-src 'none'" },
    ],
    "getWorldConfigurations() returns current CSP configurations"
  );

  // Test that we can reset a non-default world.
  await queryExtension("resetWorldConfiguration", "world_3");
  Assert.deepEqual(
    await testEvalCheckerWithUserScripts(),
    { evalOk: ["world_1"], evalBlocked: ["", "world_2", "world_3"] },
    "world_3 now defaults to the default, which still blocks eval()"
  );
  Assert.deepEqual(
    await queryExtension("getWorldConfigurations"),
    [{ worldId: "world_1", csp: "" }],
    "getWorldConfigurations() returns only remaining configuration"
  );

  // Test that when a world is configured and the CSP is omitted, that it
  // falls back to the default world.
  await queryExtension("configureWorld", { worldId: "world_2" });
  await queryExtension("configureWorld", { worldId: "world_3", csp: null });
  Assert.deepEqual(
    await testEvalCheckerWithUserScripts(),
    { evalOk: ["world_1"], evalBlocked: ["", "world_2", "world_3"] },
    "world_2 and world_3 fall back to the default world's CSP"
  );
  // Now relax the default world again.
  await queryExtension("configureWorld", { worldId: "", csp: "" });
  // Verify that it just updated the default world's csp, and not all others
  // that would fall back to the default.
  Assert.deepEqual(
    await queryExtension("getWorldConfigurations"),
    [
      { worldId: "world_1", csp: "" },
      { worldId: "world_2", csp: null },
      { worldId: "world_3", csp: null },
      { worldId: "", csp: "" },
    ],
    "getWorldConfigurations() returns null where csp was not explicitly set"
  );
  Assert.deepEqual(
    await testEvalCheckerWithUserScripts(),
    { evalOk: ["", "world_1", "world_2", "world_3"], evalBlocked: [] },
    "world_2 and world_3 use the new (relaxed) default world CSP"
  );

  // Now set the default world without explicit CSP. Should default to strict.
  await queryExtension("configureWorld", { worldId: "" });
  Assert.deepEqual(
    await testEvalCheckerWithUserScripts(),
    { evalOk: ["world_1"], evalBlocked: ["", "world_2", "world_3"] },
    "When default world does not have explicit csp, default CSP is strict"
  );

  Assert.deepEqual(
    await queryExtension("getWorldConfigurations"),
    [
      { worldId: "world_1", csp: "" },
      { worldId: "world_2", csp: null },
      { worldId: "world_3", csp: null },
      { worldId: "", csp: null },
    ],
    "getWorldConfigurations() returns null where csp was not explicitly set"
  );

  await extension.unload();
});

add_task(async function test_configuration_persists_across_restarts() {
  let { extension, queryExtension } = await startEvalTesterExtension();

  await queryExtension("registerUserScriptForWorldId", "");
  await queryExtension("registerUserScriptForWorldId", "world_1");
  await queryExtension("registerUserScriptForWorldId", "world_2");
  await queryExtension("registerUserScriptForWorldId", "world_3");

  // Default world: use defaults (fall back to default strict CSP).
  await queryExtension("configureWorld", {});
  // Set and reset world_1, just to show that there is no lingering state.
  await queryExtension("configureWorld", { worldId: "world_1", csp: "" });
  await queryExtension("resetWorldConfiguration", "world_1");
  // world_2: relax CSP.
  await queryExtension("configureWorld", { worldId: "world_2", csp: "" });
  // world_3: fall back to default, which ultimately falls back to strict CSP.
  await queryExtension("configureWorld", { worldId: "world_3", csp: null });

  // The test_world_specific_csp_override test already verified that the CSP is
  // correctly enforced immediately, so we restart immediately to verify that
  // the configurations persist.

  await AddonTestUtils.promiseShutdownManager();
  ExtensionUserScripts._getStoreForTesting()._uninitForTesting();
  await AddonTestUtils.promiseStartupManager();
  await extension.wakeupBackground();

  Assert.deepEqual(
    await queryExtension("getWorldConfigurations"),
    [
      { worldId: "", csp: null },
      { worldId: "world_2", csp: "" },
      { worldId: "world_3", csp: null },
    ],
    "getWorldConfigurations() returns null where csp was not explicitly set"
  );

  Assert.deepEqual(
    await testEvalCheckerWithUserScripts(),
    { evalOk: ["world_2"], evalBlocked: ["", "world_1", "world_3"] },
    "World-specific CSP configurations persist across restarts"
  );

  await extension.unload();
});
