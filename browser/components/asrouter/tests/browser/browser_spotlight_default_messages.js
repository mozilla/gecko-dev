"use strict";

const { AboutWelcomeTelemetry } = ChromeUtils.importESModule(
  "resource:///modules/aboutwelcome/AboutWelcomeTelemetry.sys.mjs"
);

const { OnboardingMessageProvider } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/OnboardingMessageProvider.sys.mjs"
);
const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
);

const { Spotlight } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/Spotlight.sys.mjs"
);

async function dialogClosed(browser) {
  await TestUtils.waitForCondition(
    () => !browser?.ownerGlobal.gDialogBox.isOpen
  );
}

async function getOnboardingMessageById(msgId) {
  let messages = await OnboardingMessageProvider.getMessages();
  return messages.find(({ id }) => id === msgId);
}

async function showDialog(dialogOptions) {
  Spotlight.showSpotlightDialog(
    dialogOptions.browser,
    dialogOptions.message,
    dialogOptions.dispatchStub
  );

  const [win] = await TestUtils.topicObserved("subdialog-loaded");
  return win;
}

async function setUp(messageId) {
  const message = await getOnboardingMessageById(messageId);
  const sandbox = sinon.createSandbox();
  const dispatchStub = sandbox.stub();
  const browser = BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;
  const win = await showDialog({ message, browser, dispatchStub });

  return {
    browser,
    dispatchStub,
    message,
    sandbox,
    specialActionStub: sandbox.stub(SpecialMessageActions, "handleAction"),
    spy: sandbox.spy(AboutWelcomeTelemetry.prototype, "sendTelemetry"),
    win,
  };
}

function testImpressions(dispatchStub, messageId) {
  Assert.equal(
    dispatchStub.firstCall.args[0].type,
    "SPOTLIGHT_TELEMETRY",
    "Dispatches a SPOTLIGHT_TELEMETRY event"
  );
  Assert.equal(
    dispatchStub.firstCall.args[0].data.event,
    "IMPRESSION",
    "Event is an IMPRESSSION for ASRouter frequency capping"
  );
  Assert.equal(
    dispatchStub.secondCall.args[0].type,
    "IMPRESSION",
    "Dispatches an IMPRESSION event for telemetry"
  );
  Assert.equal(
    dispatchStub.secondCall.args[0].data.id,
    messageId,
    `Event has expected id of ${messageId}`
  );
}

async function testScreenElementsRendered(win, message) {
  const screenContent = message.content.screens[0].content;

  // Wait for main content to render
  await TestUtils.waitForCondition(() =>
    win.document.querySelector(`main.${message.content.screens[0].id}`)
  );

  const selectors = [
    `main.${message.content.screens[0].id}`, // screen element
    `img[src="${screenContent.logo.imageURL}"]`, // main image
    `h1[data-l10n-id="${screenContent.title.string_id}"]`, // main title
    `h2[data-l10n-id="${screenContent.subtitle.string_id}"]`, // subtitle
    `button.primary[data-l10n-id="${screenContent.primary_button.label.string_id}"]`, // primary button
    `button.secondary[data-l10n-id="${screenContent.secondary_button.label.string_id}"]`, // secondary button
  ];

  if (screenContent.dismissButton) {
    selectors.push(`button.dismiss_button`);
  }

  for (let selector of selectors) {
    Assert.ok(
      win.document.querySelector(selector),
      `Element present with selector ${selector}`
    );
  }
}

async function testTelemetry(spy, message) {
  const screenId = message.content.screens[0].id;
  Assert.equal(
    spy.lastCall.args[0].event,
    "CLICK_BUTTON",
    "A click button event is sent after primary button click"
  );

  Assert.ok(
    spy.lastCall.args[0].message_id.startsWith(message.id) &&
      spy.lastCall.args[0].message_id.endsWith(screenId),
    `Event has a message id starting with ${message.id} and ending with ${screenId}`
  );
}

async function waitForClick(selector, win) {
  await TestUtils.waitForCondition(() => win.document.querySelector(selector));
  win.document.querySelector(selector).click();
}

/* Test Fox Doodle Set to Default message screen */
add_task(async function test_spotlight_fox_doodle_set_to_default() {
  const messageId = "FOX_DOODLE_SET_DEFAULT";
  const {
    browser,
    dispatchStub,
    message,
    sandbox,
    specialActionStub,
    spy,
    win,
  } = await setUp(messageId);

  await testScreenElementsRendered(win, message);

  // Test primary action
  await waitForClick("button.primary", win);
  await dialogClosed(browser);
  Assert.deepEqual(
    specialActionStub.firstCall.args[0].type,
    "SET_DEFAULT_BROWSER",
    "Should call set default special message action and close window on primary button click"
  );

  testTelemetry(spy, message);

  testImpressions(dispatchStub, messageId);

  // Clean up
  sandbox.restore();
});

/* Test Tail Fox Set to Default message screen */
add_task(async function test_spotlight_tail_fox_set_to_default() {
  const messageId = "TAIL_FOX_SET_DEFAULT";
  const {
    browser,
    dispatchStub,
    message,
    sandbox,
    specialActionStub,
    spy,
    win,
  } = await setUp(messageId);

  await testScreenElementsRendered(win, message);

  // Test primary action
  await waitForClick("button.primary", win);
  await dialogClosed(browser);

  Assert.deepEqual(
    specialActionStub.firstCall.args[0].type,
    "SET_DEFAULT_BROWSER",
    "Should call set default special message action and close window on primary button click"
  );

  testTelemetry(spy, message);

  testImpressions(dispatchStub, messageId);

  // Clean up
  sandbox.restore();
});
