"use strict";

const { AboutMessagePreviewParent } = ChromeUtils.importESModule(
  "resource:///actors/AboutWelcomeParent.sys.mjs"
);

/**
 * Test the parent receiveMessage function
 */
add_task(async function test_receive_message() {
  const messageSandbox = sinon.createSandbox();
  let { cleanup, browser } = await openMessagePreviewTab();
  let aboutMessagePreviewActor = await getAboutMessagePreviewParent(browser);
  messageSandbox.spy(aboutMessagePreviewActor, "receiveMessage");
  registerCleanupFunction(() => {
    messageSandbox.restore();
  });

  await aboutMessagePreviewActor.receiveMessage(
    "MessagePreview:SHOW_MESSAGE",
    {}
  );

  await aboutMessagePreviewActor.receiveMessage("MessagePreview:CHANGE_THEME");

  const { callCount } = aboutMessagePreviewActor.receiveMessage;
  let messageCall;
  let themeCall;
  for (let i = 0; i < callCount; i++) {
    const call = aboutMessagePreviewActor.receiveMessage.getCall(i);
    info(`Call #${i}: ${JSON.stringify(call.args[0])}`);
    if (call.calledWithMatch("MessagePreview:SHOW_MESSAGE")) {
      messageCall = call;
    } else if (call.calledWithMatch("MessagePreview:CHANGE_THEME")) {
      themeCall = call;
    }
  }

  Assert.greaterOrEqual(callCount, 2, `${callCount} receive spy was called`);

  Assert.equal(
    messageCall.args[0],
    "MessagePreview:SHOW_MESSAGE",
    "Got call to handle showing a message"
  );
  Assert.equal(
    themeCall.args[0],
    "MessagePreview:CHANGE_THEME",
    "Got call to handle changing the theme"
  );

  await cleanup();
});
