/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RootMessageHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs"
);

add_task(async function test_vendored_commands() {
  // Navigate to a page to make sure that the windowglobal modules run in a
  // different process than the root module.
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const browsingContextId = tab.linkedBrowser.browsingContext.id;

  const rootMessageHandler = createRootMessageHandler(
    "session-id-vendored-commands"
  );

  const valueFromRoot = await rootMessageHandler.handleCommand({
    moduleName: "vendor:vendored",
    commandName: "testRoot",
    destination: {
      type: RootMessageHandler.type,
    },
  });
  is(valueFromRoot, "valueFromRoot");

  const valueFromWindowGlobal = await rootMessageHandler.handleCommand({
    moduleName: "vendor:vendored",
    commandName: "testWindowGlobal",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContextId,
    },
  });
  is(valueFromWindowGlobal, "valueFromWindowGlobal");

  const valueFromWindowGlobalInRoot = await rootMessageHandler.handleCommand({
    moduleName: "vendor:vendored",
    commandName: "testWindowGlobalInRoot",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContextId,
    },
  });
  is(valueFromWindowGlobalInRoot, "valueFromWindowGlobalInRoot");

  rootMessageHandler.destroy();
  gBrowser.removeTab(tab);
});

add_task(async function test_vendored_events_dispatcher() {
  // Navigate to a page to make sure that the windowglobal modules run in a
  // different process than the root module.
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const browsingContext = tab.linkedBrowser.browsingContext;
  const browsingContextId = browsingContext.id;

  const rootMessageHandler = createRootMessageHandler(
    "session-id-vendored-events-dispatcher"
  );
  const events = [];
  const onEvent = (event, data) => events.push(data);
  await rootMessageHandler.eventsDispatcher.on(
    "vendor:vendored.vendoredWindowGlobalEvent",
    {
      type: ContextDescriptorType.TopBrowsingContext,
      id: browsingContext.browserId,
    },
    onEvent
  );

  await rootMessageHandler.handleCommand({
    moduleName: "vendor:vendored",
    commandName: "emitVendoredWindowGlobalEvent",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContextId,
    },
  });

  await BrowserTestUtils.waitForCondition(() => events.length === 1);
  is(events[0], "vendoredWindowGlobalEventValue");

  await rootMessageHandler.eventsDispatcher.on(
    "vendor:vendored.vendoredRootEvent",
    {
      type: ContextDescriptorType.All,
    },
    onEvent
  );

  await rootMessageHandler.handleCommand({
    moduleName: "vendor:vendored",
    commandName: "emitVendoredRootEvent",
    destination: {
      type: RootMessageHandler.type,
    },
  });

  await BrowserTestUtils.waitForCondition(() => events.length === 2);
  is(events[1], "vendoredRootEventValue");

  rootMessageHandler.destroy();
  gBrowser.removeTab(tab);
});

add_task(async function test_vendored_session_data() {
  // Navigate to a page to make sure that the windowglobal modules run in a
  // different process than the root module.
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const browsingContextId = tab.linkedBrowser.browsingContext.id;

  const rootMessageHandler = createRootMessageHandler(
    "session-id-vendored-session-data"
  );

  await rootMessageHandler.updateSessionData([
    {
      method: "add",
      moduleName: "vendor:vendored",
      category: "vendored-session-data",
      contextDescriptor: contextDescriptorAll,
      values: ["vendored-session-data-value"],
    },
  ]);

  const valueFromSessionData = await rootMessageHandler.handleCommand({
    moduleName: "vendor:vendored",
    commandName: "getSessionDataValue",
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContextId,
    },
  });
  is(valueFromSessionData[0].value, "vendored-session-data-value");

  rootMessageHandler.destroy();
  gBrowser.removeTab(tab);
});

add_task(async function test_vendored_unknown_modules() {
  // Navigate to a page to make sure that the windowglobal modules run in a
  // different process than the root module.
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  const rootMessageHandler = createRootMessageHandler(
    "session-id-vendored-commands"
  );

  info("Attempt to call a vendored module with an unknown vendor prefix");
  Assert.throws(
    () =>
      rootMessageHandler.handleCommand({
        moduleName: "rodnev:vendored",
        commandName: "testRoot",
        destination: {
          type: RootMessageHandler.type,
        },
      }),
    err =>
      err.name == "UnsupportedCommandError" &&
      err.message ==
        `rodnev:vendored.testRoot not supported for destination ROOT`
  );

  info("Attempt to call an unknown vendored module with a valid vendor prefix");
  Assert.throws(
    () =>
      rootMessageHandler.handleCommand({
        moduleName: "vendor:derodnev",
        commandName: "testRoot",
        destination: {
          type: RootMessageHandler.type,
        },
      }),
    err =>
      err.name == "UnsupportedCommandError" &&
      err.message ==
        `vendor:derodnev.testRoot not supported for destination ROOT`
  );

  rootMessageHandler.destroy();
  gBrowser.removeTab(tab);
});
