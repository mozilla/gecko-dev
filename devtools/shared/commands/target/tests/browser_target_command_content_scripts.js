/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API around workers

const FISSION_TEST_URL = URL_ROOT_SSL + "fission_document.html";

add_task(async function () {
  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  await pushPref("devtools.debugger.show-content-scripts", true);

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Addon with content script",
      content_scripts: [
        {
          matches: [`https://example.com/*`],
          js: ["content-script.js"],
        },
      ],
    },
    files: {
      "content-script.js": function () {
        browser.test.notifyPass("contentScriptRan");
      }
    }
  });

  await extension.startup();

  info("Test TargetCommand against content scripts via a tab target");
  const tab = await addTab(FISSION_TEST_URL);

  await extension.awaitFinish("contentScriptRan");

  // Create a TargetCommand for the tab
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;

  await commands.targetCommand.startListening();

  const { TYPES } = targetCommand;

  info("Check that getAllTargets only returns content scripts");
  const contentScripts = await targetCommand.getAllTargets([
    TYPES.CONTENT_SCRIPT,
  ]);

  is(contentScripts.length, 1, "Retrieved the content script");
  const [contentScript] = contentScripts;
  Assert.stringContains(contentScript.title, "Addon with content script");

  info(
    "Assert that watchTargets works for the existing content script"
  );
  const targets = [];
  const destroyedTargets = [];
  const onAvailable = async ({ targetFront }) => {
    info(`onAvailable called for ${targetFront.title}`);
    is(
      targetFront.targetType,
      TYPES.CONTENT_SCRIPT,
      "We are only notified about content script targets"
    );
    ok(!targetFront.isTopLevel, "The content scripts are never top level");
    targets.push(targetFront);
    info(`Handled ${targets.length} targets\n`);
  };
  const onDestroyed = async ({ targetFront }) => {
    is(
      targetFront.targetType,
      TYPES.CONTENT_SCRIPT,
      "We are only notified about content script targets"
    );
    destroyedTargets.push(targetFront);
  };

  await targetCommand.watchTargets({
    types: [TYPES.CONTENT_SCRIPT],
    onAvailable,
    onDestroyed,
  });

  is (targets.length, 1, "watchTargets notifies about a unique target");
  is(targets[0], contentScript, "watchTargets reports the same target instance");

  await reloadBrowser();

  await waitFor(() => destroyedTargets.length == 1, "Wait for content script target to be destroyed on navigation");
  await waitFor(() => targets.length == 2, "Wait for a new content script target to be created on navigation");

  is(destroyedTargets[0], contentScript, "the previous target is destroyed");
  is(targets.length, 2, "a new target is notified");
  Assert.stringContains(targets[1].title, "Addon with content script", "the new target is still about the same content script");

  await extension.unload();

  await waitFor(() => destroyedTargets.length == 2, "Content scripts are destroyed on extension destruction");

  targetCommand.unwatchTargets({
    types: [TYPES.CONTENT_SCRIPT],
    onAvailable,
    onDestroyed,
  });
  targetCommand.destroy();

  BrowserTestUtils.removeTab(tab);
  await commands.destroy();
});
