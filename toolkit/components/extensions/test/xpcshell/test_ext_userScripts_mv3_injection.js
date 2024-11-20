"use strict";

const server = createHttpServer({ hosts: ["example.com", "example.net"] });

server.registerPathHandler("/resultCollector", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  // main world scripts will append to resultCollector.
  response.write(`<script>globalThis.resultCollector = [];</script>>`);
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

async function collectResults(contentPage) {
  return contentPage.spawn([], () => {
    return this.content.wrappedJSObject.resultCollector;
  });
}

add_setup(async () => {
  Services.prefs.setBoolPref("extensions.userScripts.mv3.enabled", true);
  await ExtensionTestUtils.startAddonManager();
});

// Tests that user scripts can run in the MAIN world, and only for new document
// loads (not existing ones).
add_task(async function userScript_runs_in_MAIN_world() {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      manifest_version: 3,
      permissions: ["userScripts"],
      host_permissions: ["*://example.com/*"],
    },
    files: {
      "3.file.js": "resultCollector.push('3.file');dump('3.file.js ran\\n');",
      "6.file.js": "resultCollector.push('6.file');dump('6.file.js ran\\n');",
    },
    async background() {
      await browser.userScripts.register([
        {
          id: "basic",
          matches: ["*://example.com/resultCollector"],
          js: [
            { code: "resultCollector.push('1.code');dump('1.code ran\\n');" },
            { code: "resultCollector.push('2.code');dump('2.code ran\\n');" },
            { file: "3.file.js" },
            { code: "resultCollector.push('4.code');dump('4.code ran\\n');" },
            { code: "resultCollector.push('5.code');dump('5.code ran\\n');" },
            { file: "6.file.js" },
          ],
          runAt: "document_end",
          world: "MAIN",
        },
      ]);
      browser.test.sendMessage("registered");
    },
  });

  let contentPageBeforeExtStarted = await ExtensionTestUtils.loadContentPage(
    "http://example.com/resultCollector"
  );

  await extension.startup();
  await extension.awaitMessage("registered");

  let contentPageAfterRegister = await ExtensionTestUtils.loadContentPage(
    "http://example.com/resultCollector"
  );
  Assert.deepEqual(
    await collectResults(contentPageAfterRegister),
    ["1.code", "2.code", "3.file", "4.code", "5.code", "6.file"],
    "All MAIN world scripts executed in a new page after registration"
  );
  Assert.deepEqual(
    await collectResults(contentPageBeforeExtStarted),
    [],
    "User scripts did not execute in content that existed before registration"
  );

  await contentPageAfterRegister.close();
  await contentPageBeforeExtStarted.close();

  await extension.unload();
});

add_task(async function userScript_require_host_permissions() {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      manifest_version: 3,
      permissions: ["userScripts"],
      host_permissions: ["*://example.net/*"],
    },
    async background() {
      await browser.userScripts.register([
        {
          id: "basic",
          matches: ["*://*/resultCollector"],
          js: [{ code: "resultCollector.push(origin)" }],
          runAt: "document_end",
          world: "MAIN",
        },
        {
          // For extra coverage use includeGlobs without matches, since most
          // other tests do use "matches". The underlying userScripts-specific
          // matches+includeGlobs matching logic is extensively covered by
          // test_WebExtensionContentScript_isUserScript in
          // test_WebExtensionContentScript.js.
          id: "includeGlobs without matches",
          includeGlobs: ["*resultCollector"],
          js: [{ code: "resultCollector.push(origin)" }],
          runAt: "document_end",
          world: "MAIN",
        },
      ]);
      browser.test.sendMessage("registered");
    },
  });

  await extension.startup();
  await extension.awaitMessage("registered");

  {
    let contentPage = await ExtensionTestUtils.loadContentPage(
      "http://example.net/resultCollector"
    );
    Assert.deepEqual(
      await collectResults(contentPage),
      ["http://example.net", "http://example.net"],
      "Can execute with host permissions"
    );
    await contentPage.close();
  }

  {
    let contentPage = await ExtensionTestUtils.loadContentPage(
      "http://example.com/resultCollector"
    );
    Assert.deepEqual(
      await collectResults(contentPage),
      [],
      "Cannot execute without host permissions"
    );
    await contentPage.close();
  }

  await extension.unload();
});
