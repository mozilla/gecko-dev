/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// The expected timeouts are reduced for this test, but we still need to wait
// for the commands to fully resolve, and it might be a bit slow.
requestLongerTimeout(2);

const { Log } = ChromeUtils.importESModule(
  "resource://gre/modules/Log.sys.mjs"
);

class TestAppender extends Log.Appender {
  constructor(formatter) {
    super(formatter);
    this.messages = [];
  }

  append(message) {
    this.doAppend(message);
  }

  doAppend(message) {
    this.messages.push(message);
  }

  reset() {
    this.messages = [];
  }
}

add_task(async function test_commandTimeout() {
  const log = Log.repository.getLogger("RemoteAgent");
  const appender = new TestAppender(new Log.BasicFormatter());
  appender.level = Log.Level.Trace;
  log.addAppender(appender);

  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );
  const browsingContext = tab.linkedBrowser.browsingContext;
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  const rootMessageHandler = createRootMessageHandler("session-id-timeout");

  info("Call a relatively fast command which should not trigger a ping");
  await rootMessageHandler.handleCommand({
    moduleName: "timeout",
    commandName: "waitFor",
    params: {
      delay: 1000,
    },
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContext.id,
    },
  });

  assertSendingPingMessageLogged(appender.messages, false);
  appender.reset();

  info("Call a slow command which will trigger a ping");
  await rootMessageHandler.handleCommand({
    moduleName: "timeout",
    commandName: "waitFor",
    params: {
      delay: 5000,
    },
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContext.id,
    },
  });

  // The command should take more time than the (reduced) timeout and a ping
  // should be sent. The ping should succeed since the content process / main
  // thread should not hang.
  assertSendingPingMessageLogged(appender.messages, true);
  assertSuccessfulPingMessageLogged(appender.messages, true);
  assertFailedPingMessageLogged(appender.messages, false);
  appender.reset();

  info("Call a command which hangs the content process main thread for 10s");
  const onBlockedCommand = rootMessageHandler.handleCommand({
    moduleName: "timeout",
    commandName: "blockProcess",
    params: {
      delay: 10000,
    },
    destination: {
      type: WindowGlobalMessageHandler.type,
      id: browsingContext.id,
    },
  });

  info("Wait until the ping message has been logged");
  await TestUtils.waitForCondition(() =>
    appender.messages.find(m =>
      m.message.startsWith("MessageHandlerFrameParent ping")
    )
  );

  // This time expect messages logged for a failed Ping.
  assertSendingPingMessageLogged(appender.messages, true);
  assertSuccessfulPingMessageLogged(appender.messages, false);
  assertFailedPingMessageLogged(appender.messages, true);
  appender.reset();

  await onBlockedCommand;

  rootMessageHandler.destroy();
  gBrowser.removeTab(tab);
});

/**
 * Assert helper to check if the message corresponding to MessageHandler
 * sending a ping can be found or not in the provided messages array.
 *
 * @param {Array} messages
 *     The array of log appender messages to check.
 * @param {boolean} expected
 *     True if the message expects to be found, false otherwise.
 */
function assertSendingPingMessageLogged(messages, expected) {
  is(
    !!messages.find(m => m.message.includes("sending ping")),
    expected,
    expected ? "A ping was sent and logged" : "No ping was sent"
  );
}

/**
 * Assert helper to check if the message corresponding to a MessageHandler
 * successful ping can be found or not in the provided messages array.
 *
 * @param {Array} messages
 *     The array of log appender messages to check.
 * @param {boolean} expected
 *     True if the message expects to be found, false otherwise.
 */
function assertSuccessfulPingMessageLogged(messages, expected) {
  is(
    !!messages.find(m =>
      /MessageHandlerFrameParent ping for command [a-zA-Z.]+ to \w+ was successful/.test(
        m.message
      )
    ),
    expected,
    expected ? "Successful ping logged" : "No successful ping logged"
  );
}

/**
 * Assert helper to check if the message corresponding to a MessageHandler
 * failed ping can be found or not in the provided messages array.
 *
 * @param {Array} messages
 *     The array of log appender messages to check.
 * @param {boolean} expected
 *     True if the message expects to be found, false otherwise.
 */
function assertFailedPingMessageLogged(messages, expected) {
  is(
    !!messages.find(m =>
      /MessageHandlerFrameParent ping for command [a-zA-Z.]+ to \w+ timed out/.test(
        m.message
      )
    ),
    expected,
    expected ? "Failed ping logged" : "No failed ping logged"
  );
}
