/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  sinon: "resource://testing-common/Sinon.sys.mjs",
  NewTabMessaging: "resource://newtab/lib/NewtabMessaging.sys.mjs",
});

function createMockSubject(targetBrowser, message, dispatch) {
  return {
    wrappedJSObject: { targetBrowser, message, dispatch },
  };
}

add_task(async function test_NewTabMessaging() {
  let messaging = new NewTabMessaging();
  let sandbox = sinon.createSandbox();
  let mockDispatch = sandbox.spy();
  messaging.store = {
    dispatch: sandbox.spy(),
    getState() {
      return this.state;
    },
  };

  // Ensure uninitialized state
  Assert.ok(!messaging.initialized, "Should not be initialized initially");

  // Initialize
  messaging.init();
  Assert.ok(messaging.initialized, "Should be initialized");

  // Fake observer notification
  let mockMessage = { id: "test-message" };
  let mockBrowser = {
    browsingContext: {
      currentWindowGlobal: {
        getActor: () => ({
          getTabDetails: () => ({ portID: "12345" }),
        }),
      },
    },
  };

  messaging.observe(
    createMockSubject(mockBrowser, mockMessage, mockDispatch),
    "newtab-message",
    null
  );

  // Check if ASRouterDispatch was set
  Assert.equal(
    messaging.ASRouterDispatch,
    mockDispatch,
    "ASRouterDispatch should be assigned"
  );

  // Simulate impression handling
  messaging.handleImpression(mockMessage);
  Assert.ok(
    mockDispatch.calledWithMatch({ type: "IMPRESSION", data: mockMessage }),
    "Impression action should be dispatched"
  );

  // Simulate telemetry
  messaging.sendTelemetry("CLICK", mockMessage);
  Assert.ok(
    mockDispatch.calledWithMatch({
      type: "NEWTAB_MESSAGE_TELEMETRY",
      data: sandbox.match.has("event", "CLICK"),
    }),
    "Telemetry event should be dispatched"
  );
  sandbox.restore();
});
