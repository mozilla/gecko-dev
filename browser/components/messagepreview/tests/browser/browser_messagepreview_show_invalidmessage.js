"use strict";

const { AboutMessagePreviewParent } = ChromeUtils.importESModule(
  "resource:///actors/AboutWelcomeParent.sys.mjs"
);

const TEST_INVALID_MESSAGE = {
  content: {
    tiles: {
      type: "addons-picker",
    },
    title: {
      string_id: "amo-picker-title",
    },
    subtitle: {
      string_id: "amo-picker-subtitle",
    },
    secondary_button: {
      label: {
        string_id: "onboarding-not-now-button-label",
      },
      style: "secondary",
      action: {
        navigate: true,
      },
    },
  },
};

add_task(async function test_show_invalid_message() {
  const messageSandbox = sinon.createSandbox();
  let { cleanup, browser } = await openMessagePreviewTab();
  let aboutMessagePreviewActor = await getAboutMessagePreviewParent(browser);
  messageSandbox.spy(aboutMessagePreviewActor, "showMessage");
  registerCleanupFunction(() => {
    messageSandbox.restore();
  });

  await aboutMessagePreviewActor.receiveMessage({
    name: "MessagePreview:SHOW_MESSAGE",
    data: JSON.stringify(TEST_INVALID_MESSAGE),
    validationEnabled: false,
  });

  const { callCount } = aboutMessagePreviewActor.showMessage;

  Assert.greaterOrEqual(callCount, 1, "showMessage was called");
  ok(console.error, "Should throw an error");

  await cleanup();
});
