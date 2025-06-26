"use strict";

const { AboutMessagePreviewParent } = ChromeUtils.importESModule(
  "resource:///actors/AboutWelcomeParent.sys.mjs"
);

const TEST_CFR_MESSAGE = {
  content: {
    text: {
      string_id: "cfr-doorhanger-video-support-body",
    },
    layout: "icon_and_message",
    buttons: {
      primary: {
        label: {
          string_id: "cfr-doorhanger-video-support-primary-button",
        },
        action: {
          data: {
            args: "https://support.mozilla.org/kb/update-firefox-latest-release",
            where: "tabshifted",
          },
          type: "OPEN_URL",
        },
      },
      secondary: [
        {
          label: {
            string_id: "cfr-doorhanger-extension-cancel-button",
          },
          action: {
            type: "CANCEL",
          },
        },
      ],
    },
    category: "cfrFeatures",
    anchor_id: "PanelUI-menu-button",
    bucket_id: "CFR_FULL_VIDEO_SUPPORT_EN",
    info_icon: {
      label: {
        string_id: "cfr-doorhanger-extension-sumo-link",
      },
    },
    heading_text: {
      string_id: "cfr-doorhanger-video-support-header",
    },
    notification_text: "Message from Firefox",
    persistent_doorhanger: true,
    skip_address_bar_notifier: true,
  },
  trigger: {
    id: "openURL",
  },
  template: "cfr_doorhanger",
  targeting: "true",
  id: "CFR_FULL_VIDEO_SUPPORT_EN",
};

add_task(async function test_show_cfr_message() {
  const messageSandbox = sinon.createSandbox();
  let { cleanup, browser } = await openMessagePreviewTab();
  let aboutMessagePreviewActor = await getAboutMessagePreviewParent(browser);
  messageSandbox.spy(aboutMessagePreviewActor, "showMessage");
  registerCleanupFunction(() => {
    messageSandbox.restore();
  });

  await aboutMessagePreviewActor.receiveMessage({
    name: "MessagePreview:SHOW_MESSAGE",
    data: JSON.stringify(TEST_CFR_MESSAGE),
  });

  const { callCount } = aboutMessagePreviewActor.showMessage;
  Assert.greaterOrEqual(callCount, 1, "showMessage was called");

  // Wait for the CFR to show
  await BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");

  Assert.ok(
    document.getElementById("contextual-feature-recommendation-notification")
      .hidden === false,
    "Panel should be visible"
  );

  await clearNotifications();
  await cleanup();
});
