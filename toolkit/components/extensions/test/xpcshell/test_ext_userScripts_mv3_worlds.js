"use strict";

const server = createHttpServer({ hosts: ["example.com", "example.net"] });
server.registerPathHandler("/dummy", () => {});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

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
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      manifest_version: 3,
      permissions: ["userScripts"],
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
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      manifest_version: 3,
      permissions: ["userScripts"],
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
