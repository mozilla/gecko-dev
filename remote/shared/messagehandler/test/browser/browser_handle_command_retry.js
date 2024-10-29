/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { isInitialDocument } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/transports/BrowsingContextUtils.sys.mjs"
);
const { RootMessageHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs"
);

// We are forcing the actors to shutdown while queries are unresolved.
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Actor 'MessageHandlerFrame' destroyed before query 'MessageHandlerFrameParent:sendCommand' was resolved/
);

// The tests in this file assert the retry behavior for MessageHandler commands.
// We call "blocked" commands from resources/modules/windowglobal/retry.sys.mjs
// and then trigger reload and navigations to simulate AbortErrors and force the
// MessageHandler to retry the commands, when possible.

// If no retryOnAbort argument is provided, the framework will retry automatically.
add_task(async function test_default_retry() {
  let tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );

  let rootMessageHandler = createRootMessageHandler("session-id-retry");

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

  // Now try again with a normal navigation which has to retry as well.

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

    await onBlockedOneTime;
  } finally {
    await cleanup(rootMessageHandler, tab);
  }
});

// Test that without retry behavior, a pending command rejects when the
// underlying JSWindowActor pair is destroyed.
add_task(async function test_forced_no_retry() {
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const browsingContext = tab.linkedBrowser.browsingContext;

  const rootMessageHandler = createRootMessageHandler("session-id-no-retry");

  try {
    info("Call a module method which will throw");
    const onBlockedOneTime = rootMessageHandler.handleCommand({
      moduleName: "retry",
      commandName: "blockedOneTime",
      destination: {
        type: WindowGlobalMessageHandler.type,
        id: browsingContext.id,
      },
      retryOnAbort: false,
    });

    // Reloading the tab will reject the pending query with an AbortError.
    await BrowserTestUtils.reloadTab(tab);

    await Assert.rejects(
      onBlockedOneTime,
      e => e.name == "DiscardedBrowsingContextError",
      "Caught the expected error when reloading"
    );
  } finally {
    await cleanup(rootMessageHandler, tab);
  }
});

// Test that without retry behavior, a pending command rejects when the
// underlying browsing context is discarded.
add_task(async function test_forced_no_retry_cross_group() {
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=COM" +
      // Attach an unload listener to prevent the page from going into bfcache,
      // so that pending queries will be rejected with an AbortError.
      "<script type='text/javascript'>window.onunload = function() {};</script>"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const browsingContext = tab.linkedBrowser.browsingContext;

  const rootMessageHandler = createRootMessageHandler("session-id-no-retry");

  try {
    const onBlockedOneTime = rootMessageHandler.handleCommand({
      moduleName: "retry",
      commandName: "blockedOneTime",
      destination: {
        type: WindowGlobalMessageHandler.type,
        id: browsingContext.id,
      },
      retryOnAbort: false,
    });

    // This command will return when the old browsing context was discarded.
    const onDiscarded = rootMessageHandler.handleCommand({
      moduleName: "retry",
      commandName: "waitForDiscardedBrowsingContext",
      destination: {
        type: RootMessageHandler.type,
      },
      params: {
        browsingContext,
        retryOnAbort: false,
      },
    });

    ok(
      !(await hasPromiseResolved(onBlockedOneTime)),
      "blockedOneTime should not have resolved yet"
    );
    ok(
      !(await hasPromiseResolved(onDiscarded)),
      "waitForDiscardedBrowsingContext should not have resolved yet"
    );

    info(
      "Navigate to example.net with COOP headers to destroy browsing context"
    );
    await loadURL(
      tab.linkedBrowser,
      "https://example.net/document-builder.sjs?headers=Cross-Origin-Opener-Policy:same-origin&html=NET"
    );

    await Assert.rejects(
      onBlockedOneTime,
      e => e.name == "DiscardedBrowsingContextError",
      "Caught the expected error when navigating"
    );

    await Assert.rejects(
      onDiscarded,
      e => e.name == "DiscardedBrowsingContextError",
      "Caught the expected error when navigating"
    );
  } finally {
    await cleanup(rootMessageHandler, tab);
  }
});

// Test various commands, which all need a different number of "retries" to
// succeed. Check that they only resolve when the expected number of "retries"
// was reached. For commands which require more "retries" than we allow, check
// that we still fail with an AbortError once all the attempts are consumed.
add_task(async function test_forced_retry() {
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const browsingContext = tab.linkedBrowser.browsingContext;

  const rootMessageHandler = createRootMessageHandler("session-id-retry");

  try {
    // This command will return if called twice.
    const onBlockedOneTime = rootMessageHandler.handleCommand({
      moduleName: "retry",
      commandName: "blockedOneTime",
      destination: {
        type: WindowGlobalMessageHandler.type,
        id: browsingContext.id,
      },
      params: {
        foo: "bar",
      },
      retryOnAbort: true,
    });

    // This command will return if called three times.
    const onBlockedTenTimes = rootMessageHandler.handleCommand({
      moduleName: "retry",
      commandName: "blockedTenTimes",
      destination: {
        type: WindowGlobalMessageHandler.type,
        id: browsingContext.id,
      },
      params: {
        foo: "baz",
      },
      retryOnAbort: true,
    });

    // This command will return if called twelve times, which is greater than the
    // maximum amount of retries allowed.
    const onBlockedElevenTimes = rootMessageHandler.handleCommand({
      moduleName: "retry",
      commandName: "blockedElevenTimes",
      destination: {
        type: WindowGlobalMessageHandler.type,
        id: browsingContext.id,
      },
      retryOnAbort: true,
    });

    info("Reload one time");
    await BrowserTestUtils.reloadTab(tab);

    info("blockedOneTime should resolve on the first retry");
    let { callsToCommand, foo } = await onBlockedOneTime;
    is(
      callsToCommand,
      2,
      "The command was called twice (initial call + 1 retry)"
    );
    is(foo, "bar", "The parameter was sent when the command was retried");

    // We already reloaded 1 time. Reload 9 more times to unblock blockedTenTimes.
    for (let i = 2; i < 11; i++) {
      info("blockedTenTimes/blockedElevenTimes should not have resolved yet");
      ok(!(await hasPromiseResolved(onBlockedTenTimes)));
      ok(!(await hasPromiseResolved(onBlockedElevenTimes)));

      info(`Reload the tab (time: ${i})`);
      await BrowserTestUtils.reloadTab(tab);
    }

    info("blockedTenTimes should resolve on the 10th reload");
    ({ callsToCommand, foo } = await onBlockedTenTimes);
    is(
      callsToCommand,
      11,
      "The command was called 11 times (initial call + 10 retry)"
    );
    is(foo, "baz", "The parameter was sent when the command was retried");

    info("Reload one more time");
    await BrowserTestUtils.reloadTab(tab);

    info(
      "The call to blockedElevenTimes now exceeds the maximum attempts allowed"
    );
    await Assert.rejects(
      onBlockedElevenTimes,
      e => e.name == "DiscardedBrowsingContextError",
      "Caught the expected error when reloading"
    );
  } finally {
    await cleanup(rootMessageHandler, tab);
  }
});

// Test cross-group navigations to check that the retry mechanism will
// transparently switch to the new Browsing Context created by the cross-group
// navigation.
add_task(async function test_retry_cross_group() {
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=COM" +
      // Attach an unload listener to prevent the page from going into bfcache,
      // so that pending queries will be rejected with an AbortError.
      "<script type='text/javascript'>window.onunload = function() {};</script>"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const browsingContext = tab.linkedBrowser.browsingContext;

  const rootMessageHandler = createRootMessageHandler(
    "session-id-retry-cross-group"
  );

  try {
    // This command hangs and only returns if the current domain is example.net.
    // We send the command while on example.com, perform a series of reload and
    // navigations, and the retry mechanism should allow onBlockedOnNetDomain to
    // resolve.
    const onBlockedOnNetDomain = rootMessageHandler.handleCommand({
      moduleName: "retry",
      commandName: "blockedOnNetDomain",
      destination: {
        type: WindowGlobalMessageHandler.type,
        id: browsingContext.id,
      },
      params: {
        foo: "bar",
      },
      retryOnAbort: true,
    });

    // This command will return when the old browsing context was discarded.
    const onDiscarded = rootMessageHandler.handleCommand({
      moduleName: "retry",
      commandName: "waitForDiscardedBrowsingContext",
      destination: {
        type: RootMessageHandler.type,
      },
      params: {
        browsingContext,
        retryOnAbort: true,
      },
    });

    info("Reload one time");
    await BrowserTestUtils.reloadTab(tab);

    info("blockedOnNetDomain should not have resolved yet");
    ok(!(await hasPromiseResolved(onBlockedOnNetDomain)));

    info("waitForDiscardedBrowsingContext should not have resolved yet");
    ok(!(await hasPromiseResolved(onDiscarded)));

    info(
      "Navigate to example.net with COOP headers to destroy browsing context"
    );
    await loadURL(
      tab.linkedBrowser,
      "https://example.net/document-builder.sjs?headers=Cross-Origin-Opener-Policy:same-origin&html=NET"
    );

    info("blockedOnNetDomain should resolve now");
    let { foo } = await onBlockedOnNetDomain;
    is(foo, "bar", "The parameter was sent when the command was retried");

    info("waitForDiscardedBrowsingContext should resolve now");
    await onDiscarded;
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
