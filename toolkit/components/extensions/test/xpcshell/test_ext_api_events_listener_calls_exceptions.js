/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExtensionStorageIDB } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionStorageIDB.sys.mjs"
);

// Detect if the current build is still using the legacy storage.sync Kinto-based backend
// (currently only GeckoView builds does have that still enabled).
//
// TODO(Bug 1625257): remove this once the rust-based storage.sync backend has been enabled
// also on GeckoView build and the legacy Kinto-based backend has been ripped off.
const storageSyncKintoEnabled = Services.prefs.getBoolPref(
  "webextensions.storage.sync.kinto"
);

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));
server.registerPathHandler("/test-page.html", (req, res) => {
  res.setHeader("Content-Type", "text/html", false);
  res.write(`<!DOCTYPE html>
    <html><body><script>
      window.onerror = (evt) => {
        browser.test.log("webpage page got error event, error property set to: " + String(evt.error) + "::" +
                         evt.error?.stack + "\\n");
        window.postMessage(
          {
            message: evt.message,
            sourceName: evt.filename,
            lineNumber: evt.lineno,
            columnNumber: evt.colno,
            errorIsDefined: !!evt.error,
          },
          "*"
        );
      };
      window.errorListenerReady = true;
    </script></body></html>
  `);
});

add_setup(() => {
  // This hidden pref is only expected to be set to true if we had to revert
  // to the old behaviors due to a regression only hit once it got to release
  // and we had to set it to temporarily set to true while we are investigating
  // the underlying issue.
  //
  // Bug 1963002: remove this logic along with removing the related lazy pref getter from
  // ExtensionCommon.sys.mjs.
  const FALLBACK_HIDDEN_PREF = "extensions.callFunctionAndLogExceptionDisabled";
  Services.prefs.setBoolPref(FALLBACK_HIDDEN_PREF, true);
  if (Services.prefs.getBoolPref(FALLBACK_HIDDEN_PREF, false)) {
    info("Enabling use of ChromeUtils.callFunctionAndLogException");
    Services.prefs.setBoolPref(FALLBACK_HIDDEN_PREF, false);
  }
});

add_task(async function test_api_listener_call_exception() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "storage",
        "webRequest",
        "webRequestBlocking",
        "http://example.com/*",
      ],
      content_scripts: [
        {
          js: ["contentscript.js"],
          matches: ["http://example.com/test-page.html"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "contentscript.js": () => {
        window.onload = () => {
          browser.test.assertEq(
            window.wrappedJSObject.errorListenerReady,
            true,
            "Got an onerror listener on the content page side"
          );
          browser.test.sendMessage("contentscript-attached");
        };
        // eslint-disable-next-line mozilla/balanced-listeners
        window.addEventListener("message", evt => {
          browser.test.fail(
            `Webpage got notified on an exception raised from the content script: ${JSON.stringify(
              evt.data
            )}`
          );
        });
        // eslint-disable-next-line mozilla/balanced-listeners
        window.addEventListener("error", evt => {
          const errorDetails = {
            message: evt.message,
            sourceName: evt.filename,
            lineNumber: evt.lineno,
            columnNumber: evt.colno,
            errorIsDefined: !!evt.error,
          };
          browser.test.fail(
            `Webpage got notified on an exception raised from the content script: ${JSON.stringify(
              errorDetails
            )}`
          );
        });
        const throwAnError = () => {
          throw new Error("test-contentscript-error");
        };
        browser.storage.sync.onChanged.addListener(() => {
          throwAnError();
        });

        browser.storage.local.onChanged.addListener(() => {
          throw undefined; // eslint-disable-line no-throw-literal
        });
      },
      "extpage.html": `<!DOCTYPE html><script src="extpage.js"></script>`,
      "extpage.js": () => {
        // eslint-disable-next-line mozilla/balanced-listeners
        window.addEventListener("error", evt => {
          browser.test.log(
            `Extension page got error event, error property set to: ${evt.error} :: ${evt.error?.stack}\n`
          );
          const errorDetails = {
            message: evt.message,
            sourceName: evt.filename,
            lineNumber: evt.lineno,
            columnNumber: evt.colno,
            errorIsDefined: !!evt.error,
          };

          // Theoretically the exception thrown by a listener registered
          // from an extension webpage should be emitting an error event
          // (e.g. like for a DOM Event listener in a similar scenario),
          // but we never emitted it and so it would be better to only emit
          // it after have explicitly accepted the slightly change in behavior.
          browser.test.log(
            `extension page got notified on an exception raised from the API event listener: ${JSON.stringify(
              errorDetails
            )}`
          );
        });
        browser.webRequest.onBeforeRequest.addListener(
          () => {
            throw new Error(`Mock webRequest listener exception`);
          },
          { urls: ["http://example.com/data/*"] },
          ["blocking"]
        );

        // An object with a custom getter for the `message` property and a custom
        // toString method, both are triggering a test failure to make sure we do
        // catch with a failure if we are running the extension code as a side effect
        // of logging the error to the console service.
        const nonError = {
          // eslint-disable-next-line getter-return
          get message() {
            browser.test.fail(`Unexpected extension code executed`);
          },

          toString() {
            return "Custom exception string";
          },
        };
        browser.storage.sync.onChanged.addListener(() => {
          throw nonError;
        });
        // Verify DOM event listeners behaviors on the `nonError` object being raised
        // as an exception as the WebExtensions API event listeners.
        window.addEventListener(
          "TestDOMEvent",
          () => {
            throw nonError;
          },
          { once: true }
        );
        browser.test.onMessage.addListener(msg => {
          if (msg === "dispatch-test-dom-event") {
            window.dispatchEvent(new CustomEvent("TestDOMEvent"));
            return;
          }
          browser.test.fail(`Got unxpected test message: ${msg}`);
        });

        // Throwing undefined or null is also allowed and so we cover that here as well
        // to confirm we are not making any assumption about the value being raised to
        // be always defined.
        browser.storage.local.onChanged.addListener(() => {
          throw undefined; // eslint-disable-line no-throw-literal
        });
      },
    },
  });

  await extension.startup();

  const page = await ExtensionTestUtils.loadContentPage(
    extension.extension.baseURI.resolve("extpage.html"),
    { extension }
  );

  // Prepare to collect the error reported for the exception being triggered
  // by the test itself.
  const prepareWaitForConsoleMessage = () => {
    this.content.waitForConsoleMessage = new Promise(resolve => {
      const currInnerWindowID = this.content.windowGlobalChild?.innerWindowId;
      const consoleListener = {
        QueryInterface: ChromeUtils.generateQI(["nsIConsoleListener"]),
        observe: message => {
          if (
            message instanceof Ci.nsIScriptError &&
            message.innerWindowID === currInnerWindowID
          ) {
            resolve({
              message: message.message,
              category: message.category,
              sourceName: message.sourceName,
              hasStack: !!message.stack,
            });
            Services.console.unregisterListener(consoleListener);
          }
        },
      };
      Services.console.registerListener(consoleListener);
    });
  };

  const notifyStorageSyncListener = extensionTestWrapper => {
    // The notifyListeners method from ExtensionStorageSyncKinto does use
    // the Extension class instance as the key for the storage.sync listeners
    // map, whereas ExtensionStorageSync does use the extension id instead.
    //
    // TODO(Bug 1625257): remove this once the rust-based storage.sync backend has been enabled
    // also on GeckoView build and the legacy Kinto-based backend has been ripped off.
    let listenersMapKey = storageSyncKintoEnabled
      ? extensionTestWrapper.extension
      : extensionTestWrapper.id;
    ok(
      ExtensionParent.apiManager.global.extensionStorageSync.listeners.has(
        listenersMapKey
      ),
      "Got a storage.sync onChanged listener for the test extension"
    );
    ExtensionParent.apiManager.global.extensionStorageSync.notifyListeners(
      listenersMapKey,
      {}
    );
  };

  // Retrieve the message collected from the previously created promise.
  const asyncAssertConsoleMessage = async ({
    targetPage,
    expectedErrorRegExp,
    expectedSourceName,
    shouldIncludeStack,
  }) => {
    const { message, category, sourceName, hasStack } = await targetPage.spawn(
      [],
      () => this.content.waitForConsoleMessage
    );

    ok(
      expectedErrorRegExp.test(message),
      `Got the expected error message: ${message} vs ${expectedErrorRegExp}`
    );

    Assert.deepEqual(
      { category, sourceName, hasStack },
      {
        category: "content javascript",
        sourceName: expectedSourceName,
        hasStack: shouldIncludeStack,
      },
      "Expected category and sourceName are set on the nsIScriptError"
    );
  };

  {
    info("Test exception raised by webRequest listener");
    const expectedErrorRegExp = new RegExp(
      `Error: Mock webRequest listener exception`
    );
    const expectedSourceName =
      extension.extension.baseURI.resolve("extpage.js");
    await page.spawn([], prepareWaitForConsoleMessage);
    await ExtensionTestUtils.fetch(
      "http://example.com",
      "http://example.com/data/file_sample.html"
    );
    await asyncAssertConsoleMessage({
      targetPage: page,
      expectedErrorRegExp,
      expectedSourceName,
      shouldIncludeStack: true,
    });
  }

  {
    info("Test exception raised by storage.sync listener");
    // The listener has throw an object that isn't an Error instance and
    // it also has a getter for the message property, we expect it to be
    // logged using the string returned by the native toString method.
    const expectedErrorRegExp = new RegExp(
      `uncaught exception: Custom exception string`
    );
    const expectedSourceName =
      extension.extension.baseURI.resolve("extpage.js");

    await page.spawn([], prepareWaitForConsoleMessage);
    notifyStorageSyncListener(extension);
    await asyncAssertConsoleMessage({
      targetPage: page,
      expectedErrorRegExp,
      expectedSourceName,
      shouldIncludeStack: true,
    });
  }

  {
    info("Test exception raised by a DOM event listener for comparison");
    // Similarly to the previous test on the storage.sync listener but originated
    // from a DOM event listener (to ensure that the WebExtensions API event
    // listeners and the DOM event listeners are behaving similarly with custom
    // error objects).
    const expectedErrorRegExp = new RegExp(
      `uncaught exception: Custom exception string`
    );
    const expectedSourceName =
      extension.extension.baseURI.resolve("extpage.js");

    await page.spawn([], prepareWaitForConsoleMessage);
    extension.sendMessage("dispatch-test-dom-event");
    await asyncAssertConsoleMessage({
      targetPage: page,
      expectedErrorRegExp,
      expectedSourceName,
      shouldIncludeStack: true,
    });
  }

  {
    info("Test exception raised by storage.local listener");
    // The listener has throw an object that isn't an Error instance and
    // it also has a getter for the message property, we expect it to be
    // logged using the string returned by the native toString method.
    const expectedErrorRegExp = new RegExp(`uncaught exception: undefined`);
    const expectedSourceName =
      extension.extension.baseURI.resolve("extpage.js");
    await page.spawn([], prepareWaitForConsoleMessage);
    ExtensionStorageIDB.notifyListeners(extension.id, {});
    await asyncAssertConsoleMessage({
      targetPage: page,
      expectedErrorRegExp,
      expectedSourceName,
      shouldIncludeStack: true,
    });
  }

  await page.close();

  info("Test content script API event listeners exception");

  const contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/test-page.html"
  );

  await extension.awaitMessage("contentscript-attached");

  {
    info("Test exception raised by content script storage.sync listener");
    // The listener has throw an object that isn't an Error instance and
    // it also has a getter for the message property, we expect it to be
    // logged using the string returned by the native toString method.
    const expectedErrorRegExp = new RegExp(`Error: test-contentscript-error`);
    const expectedSourceName =
      extension.extension.baseURI.resolve("contentscript.js");

    await contentPage.spawn([], prepareWaitForConsoleMessage);
    notifyStorageSyncListener(extension);
    await asyncAssertConsoleMessage({
      targetPage: contentPage,
      expectedErrorRegExp,
      expectedSourceName,
      shouldIncludeStack: true,
    });
  }

  {
    info("Test exception raised by content script storage.local listener");
    // The listener has throw an object that isn't an Error instance and
    // it also has a getter for the message property, we expect it to be
    // logged using the string returned by the native toString method.
    const expectedErrorRegExp = new RegExp(`uncaught exception: undefined`);
    const expectedSourceName =
      extension.extension.baseURI.resolve("contentscript.js");

    await contentPage.spawn([], prepareWaitForConsoleMessage);
    ExtensionStorageIDB.notifyListeners(extension.id, {});
    await asyncAssertConsoleMessage({
      targetPage: contentPage,
      expectedErrorRegExp,
      expectedSourceName,
      shouldIncludeStack: true,
    });
  }

  await contentPage.close();

  await extension.unload();
});
