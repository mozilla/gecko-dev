/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExtensionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionXPCShellUtils.sys.mjs"
);

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  ExtensionParent: "resource://gre/modules/ExtensionParent.sys.mjs",
});

const { createAppInfo, promiseStartupManager } = AddonTestUtils;

AddonTestUtils.init(this);
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

ExtensionTestUtils.init(this);

// Bug 1302702 - Test connect to a webextension addon
add_task(
  {
    // This test needs to run only when the extension are running in a separate
    // child process, otherwise the thread actor would pause the main process and this
    // test would get stuck.
    skip_if: () => !WebExtensionPolicy.useRemoteWebExtensions,
  },
  async function test_webextension_addon_debugging_connect() {
    await promiseStartupManager();

    // Install and start a test webextension.
    const extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      background() {
        const { browser } = this;
        browser.test.log("background script executed");
        // window is available in background scripts
        // eslint-disable-next-line no-undef
        browser.test.sendMessage("background page ready", window.location.href);
      },
    });
    await extension.startup();
    const bgPageURL = await extension.awaitMessage("background page ready");

    const commands = await CommandsFactory.forAddon(extension.id);
    const { targetCommand } = commands;

    // Connect to the target addon actor and wait for the updated list of frames.
    await targetCommand.startListening();
    const topTarget = targetCommand.targetFront;
    const selectedTarget = targetCommand.selectedTargetFront;

    equal(
      topTarget.isFallbackExtensionDocument,
      true,
      "The top target is about the fallback document"
    );
    equal(
      selectedTarget.isFallbackExtensionDocument,
      false,
      "The background page target is automatically selected"
    );
    equal(selectedTarget.url, bgPageURL, "The background page url is correct");

    const threadFront = await topTarget.getFront("thread");

    ok(threadFront, "Got a threadFront for the target addon");
    equal(threadFront.paused, false, "The addon threadActor isn't paused");

    equal(
      lazy.ExtensionParent.DebugUtils.debugBrowserPromises.size,
      1,
      "The expected number of debug browser has been created by the addon actor"
    );

    // Reload the addon through the RDP protocol.
    await targetCommand.reloadTopLevelTarget();

    info("Wait background page to be fully reloaded");
    await extension.awaitMessage("background page ready");

    equal(
      lazy.ExtensionParent.DebugUtils.debugBrowserPromises.size,
      1,
      "The number of debug browser has not been changed after an addon reload"
    );

    await commands.destroy();

    // Check that if we close the debugging client without uninstalling the addon,
    // the webextension debugging actor should release the debug browser.
    equal(
      lazy.ExtensionParent.DebugUtils.debugBrowserPromises.size,
      0,
      "The debug browser has been released when the RDP connection has been closed"
    );

    await extension.unload();
  }
);
