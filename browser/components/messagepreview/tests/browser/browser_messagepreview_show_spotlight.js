"use strict";

const { AboutMessagePreviewParent } = ChromeUtils.importESModule(
  "resource:///actors/AboutWelcomeParent.sys.mjs"
);

const TEST_SPOTLIGHT_MESSAGE = {
  id: "TEST_SPOTLIGHT_MESSAGE",
  template: "spotlight",
  modal: "tab",
  content: {
    id: "MULTISTAGE_SPOTLIGHT_MESSAGE",
    template: "multistage",
    screens: [
      {
        id: "SCREEN_1",
        content: {
          title: {
            raw: "Test title",
          },
          logo: {},
          primary_button: {
            label: {
              raw: "primary button",
            },
            action: {
              navigate: true,
            },
          },
          secondary_button: {
            label: {
              string_id: "onboarding-not-now-button-label",
            },
            action: {
              navigate: true,
            },
          },
          dismiss_button: {
            action: {
              dismiss: true,
            },
          },
        },
      },
    ],
  },
  trigger: {
    id: "defaultBrowserCheck",
  },
  targeting: "true",
};

add_task(async function test_show_spotlight_message() {
  const messageSandbox = sinon.createSandbox();
  let { cleanup, browser } = await openMessagePreviewTab();
  let aboutMessagePreviewActor = await getAboutMessagePreviewParent(browser);
  messageSandbox.spy(aboutMessagePreviewActor, "showMessage");
  registerCleanupFunction(() => {
    messageSandbox.restore();
  });

  await aboutMessagePreviewActor.receiveMessage({
    name: "MessagePreview:SHOW_MESSAGE",
    data: JSON.stringify(TEST_SPOTLIGHT_MESSAGE),
  });

  const { callCount } = aboutMessagePreviewActor.showMessage;
  Assert.greaterOrEqual(callCount, 1, "showMessage was called");

  const [win] = await TestUtils.topicObserved("subdialog-loaded");

  await test_window_message_content(
    win,
    "renders the test spotlight",
    "SCREEN_1",
    //Expected selectors
    [
      "main.SCREEN_1", // screen element
      "img.brand-logo", // main image
      "h1#mainContentHeader", // main title
      "button[value='primary_button']", // primary button
      "button.secondary.text-link[value='secondary_button']", // secondary button
    ]
  );

  // click to close the spotlight
  await waitForClick("button.primary", win);
  await dialogClosed(browser);

  await cleanup();
});
