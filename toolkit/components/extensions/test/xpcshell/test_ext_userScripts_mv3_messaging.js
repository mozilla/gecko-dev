"use strict";

const { ExtensionTestCommon } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionTestCommon.sys.mjs"
);

const { ExtensionUserScripts } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionUserScripts.sys.mjs"
);

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

add_setup(async () => {
  Services.prefs.setBoolPref("extensions.userScripts.mv3.enabled", true);
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_runtime_messaging_errors() {
  const extensionId = "@test_runtime_messaging_errors";
  await grantUserScriptsPermission(extensionId);
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: extensionId } },
      manifest_version: 3,
      optional_permissions: ["userScripts"],
      host_permissions: ["*://example.com/*"],
      content_scripts: [
        {
          js: ["contentscript_expose_test.js"],
          matches: ["*://example.com/dummy"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "contentscript_expose_test.js": () => {
        // userscript.js does not have browser.test, export it from here.
        // eslint-disable-next-line no-undef
        window.wrappedJSObject.contentscriptTest = cloneInto(
          browser.test,
          window,
          { cloneFunctions: true }
        );
      },
      "userscript.js": async () => {
        // Note: this is the browser.test namespace from the content script.
        // We can only pass primitive values here, because Xrays prevent the
        // content script from receiving functions, etc.
        const contentscriptTest = window.wrappedJSObject.contentscriptTest;
        function assertThrows(cb, expectedError, desc) {
          let actualErrorMessage;
          try {
            cb();
            actualErrorMessage = "Unexpectedly not thrown";
          } catch (e) {
            actualErrorMessage = e.message;
          }
          contentscriptTest.assertEq(expectedError, actualErrorMessage, desc);
        }
        async function assertRejects(promise, expectedError, desc) {
          let actualErrorMessage;
          try {
            await promise;
            actualErrorMessage = "Unexpectedly not rejected";
          } catch (e) {
            actualErrorMessage = e.message;
          }
          contentscriptTest.assertEq(expectedError, actualErrorMessage, desc);
        }

        try {
          // runtime.sendMessage tests:
          assertThrows(
            () => browser.runtime.sendMessage(),
            "runtime.sendMessage's message argument is missing",
            "sendMessage without params"
          );
          assertThrows(
            () => browser.runtime.sendMessage("extensionId@", "message"),
            "runtime.sendMessage received too many arguments",
            "sendMessage with unsupported extensionId parameter"
          );
          assertThrows(
            () => browser.runtime.sendMessage("message", {}),
            "runtime.sendMessage received too many arguments",
            "sendMessage with unsupported options parameter"
          );
          assertThrows(
            () => browser.runtime.sendMessage(location),
            "Location object could not be cloned.",
            "sendMessage with non-cloneable message"
          );
          await assertRejects(
            browser.runtime.sendMessage("msg"),
            "Could not establish connection. Receiving end does not exist.",
            "Expected error when there is no onUserScriptMessage handler"
          );

          // runtime.connect tests:
          assertThrows(
            () => browser.runtime.connect("extensionId", {}),
            "extensionId is not supported",
            "connect with unsupported extensionId parameter"
          );
          assertThrows(
            () => browser.runtime.connect("extensionId"),
            "extensionId is not supported",
            "connect with unsupported extensionId parameter and no options"
          );
          assertThrows(
            () => browser.runtime.connect({ unknownProp: true }),
            `Type error for parameter connectInfo (Unexpected property "unknownProp") for runtime.connect.`,
            "connect with unrecognized property"
          );
          assertThrows(
            () => browser.runtime.connect({}, {}),
            "Incorrect argument types for runtime.connect.",
            "connect with too many parameters"
          );
        } catch (e) {
          contentscriptTest.fail(`Unexpected error in userscript.js: ${e}`);
        }
        contentscriptTest.sendMessage("done");
      },
    },
    async background() {
      await browser.userScripts.configureWorld({ messaging: true });
      await browser.userScripts.register([
        {
          id: "error checker",
          matches: ["*://example.com/dummy"],
          js: [{ file: "userscript.js" }],
        },
      ]);
      browser.test.sendMessage("registered");
    },
  });

  await extension.startup();
  await extension.awaitMessage("registered");

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );
  await extension.awaitMessage("done");
  await contentPage.close();

  await extension.unload();
});

// This tests that runtime.sendMessage works when called from a user script.
// And that the messaging flag persists across restarts.
// Moreover, that runtime.sendMessage can wake up an event page.
add_task(async function test_onUserScriptMessage() {
  const extensionId = "@test_onUserScriptMessage";
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
      "userscript.js": async () => {
        // browser.runtime.sendMessage should be available because we call
        // userScripts.configureWorld({ messaging: true }) before page load.
        let responses = [
          await browser.runtime.sendMessage("expectPromiseResult"),
          await browser.runtime.sendMessage("expectSendResponse"),
          await browser.runtime.sendMessage("expectSendResponseAsync"),
          await browser.runtime.sendMessage("expectDefaultResponse"),
        ];
        browser.runtime.sendMessage(responses);
      },
    },
    background() {
      // Set up message listeners. The user script will send multiple messages,
      // and ultimately send a message will all responses received so far.
      // NOTE: To make sure that the functionality is independent of other
      // messaging APIs, we only register runtime.onUserScriptMessage here,
      // and no other extension messaging APIs.
      browser.runtime.onUserScriptMessage.addListener(
        (msg, sender, sendResponse) => {
          // worldId defaults to "" when not specified in userScripts.register
          // and userScripts.configureWorld. That default value should appear
          // here as sender.userScriptWorldId (also an empty string).
          browser.test.assertEq(
            "",
            sender.userScriptWorldId,
            `Expected userScriptWorldId in onUserScriptMessage for: ${msg}`
          );
          switch (msg) {
            case "expectPromiseResult":
              return Promise.resolve("Promise");
            case "expectSendResponse":
              sendResponse("sendResponse");
              return;
            case "expectSendResponseAsync":
              // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
              setTimeout(() => sendResponse("sendResponseAsync"), 50);
              return true;
            case "expectDefaultResponse":
              return;
            default:
              browser.test.assertDeepEq(
                ["Promise", "sendResponse", "sendResponseAsync", undefined],
                msg,
                "All sendMessage calls got the expected response"
              );
              browser.test.sendMessage("testRuntimeSendMessage:done");
          }
        }
      );
      browser.runtime.onInstalled.addListener(async () => {
        await browser.userScripts.configureWorld({ messaging: true });
        await browser.userScripts.register([
          {
            id: "messaging checker",
            matches: ["*://example.com/dummy"],
            js: [{ file: "userscript.js" }],
          },
        ]);
        browser.test.sendMessage("registered");
      });
    },
  });

  await extension.startup();
  await extension.awaitMessage("registered");

  async function testRuntimeSendMessage() {
    let contentPage = await ExtensionTestUtils.loadContentPage(
      "http://example.com/dummy"
    );
    await extension.awaitMessage("testRuntimeSendMessage:done");
    await contentPage.close();
  }
  info("Loading page that should trigger runtime.sendMessage");
  await testRuntimeSendMessage();

  await AddonTestUtils.promiseShutdownManager();
  ExtensionUserScripts._getStoreForTesting()._uninitForTesting();
  await AddonTestUtils.promiseStartupManager();

  // Because the background has a persistent listener (runtime.onInstalled), it
  // stays suspended after a restart.
  await extension.awaitStartup();
  ExtensionTestCommon.testAssertions.assertBackgroundStatusStopped(extension);

  // We expect that the load of the page that calls runtime.sendMessage from a
  // user script will wake it to fire runtime.onUserScriptMessage.
  info("Loading page that should load user script and wake event page");
  await testRuntimeSendMessage();

  await extension.unload();
});

// This test tests the following:
// - configureWorld() with messaging=false does not affect existing worlds.
// - after reloading the page, that the user script is run again but without
//   access to messaging APIs due to messaging=false.
// - Moreover, this also verifies that runtime.sendMessage from a user script
//   does not trigger runtime.onMessage / runtime.onMessageExternal, and that
//   even if runtime.onMessage is triggered from a content script, that it does
//   not have the userScripts-specific "sender.userScriptWorldId" field.
add_task(async function test_configureWorld_messaging_existing_world() {
  const extensionId = "@test_configureWorld_messaging_existing_world";
  await grantUserScriptsPermission(extensionId);
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: extensionId } },
      manifest_version: 3,
      optional_permissions: ["userScripts"],
      host_permissions: ["*://example.com/*"],
      content_scripts: [
        {
          js: ["contentscript_result_reporter.js"],
          matches: ["*://example.com/dummy"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "contentscript_result_reporter.js": () => {
        // exportFunction is defined in the scope of a content script.
        // eslint-disable-next-line no-undef
        window.wrappedJSObject.reportResultViaContentScript = exportFunction(
          msg => browser.runtime.sendMessage(msg),
          window
        );
      },
      "userscript.js": async () => {
        try {
          dump("Trying to call sendMessage(initial_message)\n");
          await browser.runtime.sendMessage("initial_message");
          dump("Trying to call sendMessage(after_messaging_false)\n");
          await browser.runtime.sendMessage("after_messaging_false");
          // ^ we expect runtime.sendMessage() to succeed despite configuring
          // messaging to false, because we only check the flag when the APIs
          // are initialized in the sandbox. This is consistent with Chrome's
          // behavior. Note that the specification permits this behavior (but
          // it also allows the implementation to fail if desired):
          // https://github.com/w3c/webextensions/blob/d16807376b/proposals/multiple_user_script_worlds.md#L191-L193

          dump("Reloading page\n");
          location.reload();
          // ^ after reloading the page, we expect the messaging=false flag to
          // be enforced, and the first runtime.sendMessage() call should fail
          // and fall through to the catch below.
        } catch (e) {
          window.wrappedJSObject.reportResultViaContentScript(`Result:${e}`);
        }
      },
    },
    async background() {
      let msgCount = 0;
      browser.runtime.onUserScriptMessage.addListener(async (msg, sender) => {
        ++msgCount;
        browser.test.assertEq(
          "non_default_worldId",
          sender.userScriptWorldId,
          "Expected userScriptWorldId in onUserScriptMessage"
        );
        if (msgCount === 1) {
          browser.test.assertEq("initial_message", msg, "Initial message");
          browser.test.log("Calling configureWorld with messaging=false");
          await browser.userScripts.configureWorld({
            worldId: "non_default_worldId",
            messaging: false,
          });
          return;
        }
        if (msgCount === 2) {
          browser.test.assertEq("after_messaging_false", msg, "Second message");
          return;
        }
        // After reload.
        browser.test.fail(`Unexpected onUserScriptMessage ${msgCount}: ${msg}`);
      });
      browser.runtime.onMessage.addListener((msg, sender) => {
        browser.test.assertFalse(
          "userScriptWorldId" in sender,
          "No userScriptWorldId in runtime.onMessage"
        );
        browser.test.assertEq(
          2,
          msgCount,
          "Should reach reportResultViaContentScript after reloading page"
        );
        browser.test.assertEq(
          "Result:ReferenceError: browser is not defined",
          msg,
          "Expected (error) message after reload when messaging=false"
        );
        browser.test.sendMessage("done");
      });
      browser.runtime.onMessageExternal.addListener(msg => {
        browser.test.fail(`Unexpected message: ${msg}`);
      });
      await browser.userScripts.configureWorld({ messaging: true });
      await browser.userScripts.register([
        {
          id: "Test effect of configureWorld(messaging=false) and reload",
          matches: ["*://example.com/dummy"],
          js: [{ file: "userscript.js" }],
          worldId: "non_default_worldId",
        },
      ]);
      browser.test.sendMessage("registered");
    },
  });

  await extension.startup();
  await extension.awaitMessage("registered");

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );
  await extension.awaitMessage("done");
  await contentPage.close();

  await extension.unload();
});

// This test tests that runtime.connect() works when called from a user script.
add_task(async function test_onUserScriptConnect() {
  const extensionId = "@test_onUserScriptConnect";
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
      "userscript.js": () => {
        let port = browser.runtime.connect({ name: "first_port" });
        port.onMessage.addListener(msg => {
          port.postMessage({ messageBack: msg });
        });
        port.onDisconnect.addListener(() => {
          let port2 = browser.runtime.connect({ name: "second_port" });
          port2.postMessage({ errorFromFirstPort: port.error });
          port2.disconnect();
        });
      },
    },
    background() {
      browser.runtime.onUserScriptConnect.addListener(port => {
        browser.test.assertEq(
          "connect_world",
          port.sender.userScriptWorldId,
          `Expected userScriptWorldId in onUserScriptConnect: ${port.name}`
        );
        if (port.name === "first_port") {
          port.onMessage.addListener(msg => {
            browser.test.assertDeepEq(
              { messageBack: { hi: 1 } },
              msg,
              "port.onMessage triggered from user script"
            );
            port.disconnect();
            // ^ should trigger port.onDisconnect in the user script, which
            // will signal back the status by connecting again to second_port.
          });
          port.onDisconnect.addListener(() => {
            // We should not expect a disconnect, because we call
            // port.disconnect() from this end.
            browser.test.fail(`Unexpected port.onDisconnect: ${port.error}`);
          });
          port.postMessage({ hi: 1 });
          return;
        }
        if (port.name === "second_port") {
          port.onMessage.addListener(msg => {
            browser.test.assertDeepEq(
              { errorFromFirstPort: null },
              msg,
              "When we disconnect first_port, other port.error should be null"
            );
            browser.test.sendMessage("port.onMessage:done");
          });
          port.onDisconnect.addListener(() => {
            browser.test.assertDeepEq(
              null,
              port.error,
              "Our port.error when other side (user script) disconnects"
            );
            browser.test.sendMessage("port.onDisconnect:done");
          });
          return;
        }
        browser.test.fail(`Unexpected port: ${port.name}`);
      });
      browser.runtime.onInstalled.addListener(async () => {
        await browser.userScripts.configureWorld({ messaging: true });
        await browser.userScripts.register([
          {
            id: "messaging checker",
            matches: ["*://example.com/dummy"],
            js: [{ file: "userscript.js" }],
            worldId: "connect_world",
          },
        ]);
        browser.test.sendMessage("registered");
      });
    },
  });

  await extension.startup();
  await extension.awaitMessage("registered");

  async function testRuntimeConnect() {
    let contentPage = await ExtensionTestUtils.loadContentPage(
      "http://example.com/dummy"
    );
    await Promise.all([
      extension.awaitMessage("port.onMessage:done"),
      extension.awaitMessage("port.onDisconnect:done"),
    ]);
    await contentPage.close();
  }
  info("Loading page that should trigger runtime.connect");
  await testRuntimeConnect();

  await AddonTestUtils.promiseShutdownManager();
  ExtensionUserScripts._getStoreForTesting()._uninitForTesting();
  await AddonTestUtils.promiseStartupManager();

  // Because the background has a persistent listener (runtime.onInstalled), it
  // stays suspended after a restart.
  await extension.awaitStartup();
  ExtensionTestCommon.testAssertions.assertBackgroundStatusStopped(extension);

  // We expect that the load of the page that calls runtime.connect from a
  // user script will wake it to fire runtime.onUserScriptConnect.
  info("Loading page that should load user script and wake event page");
  await testRuntimeConnect();

  await extension.unload();
});

// This tests:
// - That port.onDisconnect is fired when the document navigates away.
// - That runtime.connect() can be called without parameters.
add_task(
  {
    // We want to disable the bfcache and use the unload listener to force
    // that below. But on Android bfcache.allow_unload_listeners is true by
    // default, so we force the pref to false to make sure that the bfcache is
    // indeed disabled for the test page.
    pref_set: [["docshell.shistory.bfcache.allow_unload_listeners", false]],
  },
  async function test_onUserScriptConnect_port_disconnect_on_navigate() {
    const extensionId = "@test_onUserScriptConnect_port_disconnect_on_navigate";
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
        "userscript.js": () => {
          let port = browser.runtime.connect();

          // Prevent bfcache from keeping doc + port alive.
          // eslint-disable-next-line mozilla/balanced-listeners
          window.addEventListener("unload", () => {});

          // Global var to avoid garbage collection:
          globalThis.portReference = port;

          port.onMessage.addListener(msg => {
            dump(`Will navigate elsewhere after bye message: ${msg}\n`);
            // Change URL. Note: not matched by matches[] above.
            location.search = "?something_else";
            // ^ Note: we expect the context to unload. If the test times out
            // after this point, it may be due to the page unexpectedly not
            // being unloaded, e.g. due to it being stored in the bfcache.
            // We disable the bfcache with the unload listener above.
          });
        },
      },
      async background() {
        browser.runtime.onUserScriptConnect.addListener(port => {
          browser.test.assertEq("", port.name, "Got default port.name");
          browser.test.assertEq(
            "",
            port.sender.userScriptWorldId,
            "Got default userScriptWorldId"
          );
          port.onDisconnect.addListener(() => {
            browser.test.assertDeepEq(
              null,
              port.error,
              "Closing port due to a navigation is not an error"
            );
            browser.test.sendMessage("done");
          });
          port.postMessage("bye");
        });
        await browser.userScripts.configureWorld({ messaging: true });
        await browser.userScripts.register([
          {
            id: "messaging checker",
            matches: ["*://example.com/dummy"],
            js: [{ file: "userscript.js" }],
          },
        ]);
        browser.test.sendMessage("registered");
      },
    });

    await extension.startup();
    await extension.awaitMessage("registered");

    let contentPage = await ExtensionTestUtils.loadContentPage(
      "http://example.com/dummy"
    );
    await extension.awaitMessage("done");
    await contentPage.close();
    await extension.unload();
  }
);
