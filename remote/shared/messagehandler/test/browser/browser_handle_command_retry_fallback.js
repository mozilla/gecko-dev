/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { isInitialDocument } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/transports/BrowsingContextUtils.sys.mjs"
);

// We are forcing the actors to shutdown while queries are unresolved.
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Actor 'MessageHandlerFrame' destroyed before query 'MessageHandlerFrameParent:sendCommand' was resolved/
);

// The tests in this file assert the fallback retry behavior for MessageHandler commands.
// We call "blocked" commands from resources/modules/windowglobal/retry.sys.mjs
// and then trigger reload and navigations to simulate AbortErrors and force the
// MessageHandler to retry the commands, when requested.

// If no retryOnAbort argument is provided, and the preference "remote.retry-on-abort"
// is set to false the framework should retry only if context is on the initial document.
add_task(async function test_default_fallback_retry_initial_document_only() {
  let tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );

  let rootMessageHandler = createRootMessageHandler(
    "session-id-retry-initial-document"
  );

  try {
    const initialBrowsingContext = tab.linkedBrowser.browsingContext;
    ok(
      isInitialDocument(initialBrowsingContext),
      "Module method needs to run in the initial document"
    );

    info("Call a module method which will throw");
    const onBlockedOneTime = rootMessageHandler.handleCommand({
      moduleName: "retry",
      commandName: "blockedOneTime",
      destination: {
        type: WindowGlobalMessageHandler.type,
        id: initialBrowsingContext.id,
      },
    });

    await onBlockedOneTime;

    ok(
      !isInitialDocument(tab.linkedBrowser.browsingContext),
      "module method to be successful"
    );
  } finally {
    await cleanup(rootMessageHandler, tab, false);
  }

  // Now try again with a normal navigation which should not retry.

  tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  try {
    rootMessageHandler = createRootMessageHandler("session-id-no-retry");
    const browsingContext = tab.linkedBrowser.browsingContext;

    info("Call a module method which will throw");
    const onBlockedOneTime = rootMessageHandler.handleCommand({
      moduleName: "retry",
      commandName: "blockedOneTime",
      destination: {
        type: WindowGlobalMessageHandler.type,
        id: browsingContext.id,
      },
    });

    // Reloading the tab will reject the pending query with an AbortError.
    await BrowserTestUtils.reloadTab(tab);

    await Assert.rejects(
      onBlockedOneTime,
      e => e.name == "AbortError",
      "Caught the expected abort error when reloading"
    );
  } finally {
    await cleanup(rootMessageHandler, tab);
  }
});

async function cleanup(rootMessageHandler, tab) {
  const browsingContext = tab.linkedBrowser.browsingContext;
  // Cleanup global JSM state in the test module.
  await rootMessageHandler.handleCommand({
    moduleName: "retry",
    commandName: "cleanup",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContext.id,
    },
  });

  rootMessageHandler.destroy();
  gBrowser.removeTab(tab);
}
