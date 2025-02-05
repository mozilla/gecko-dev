"use strict";

const { AboutMessagePreviewParent } = ChromeUtils.importESModule(
  "resource:///actors/AboutWelcomeParent.sys.mjs"
);

const TEST_PB_MESSAGE = {
  weight: 100,
  id: "PB_NEWTAB_TEST",
  template: "pb_newtab",
  content: {
    promoEnabled: true,
    promoType: "VPN",
    infoEnabled: false,
    infoTitleEnabled: false,
    promoLinkType: "button",
    promoLinkText: "Link",
    promoSectionStyle: "below-search",
    promoHeader: "This is a test PB message",
    promoTitle: "Test",
    promoTitleEnabled: true,
    promoImageLarge: "chrome://browser/content/assets/moz-vpn.svg",
    promoButton: {
      action: {
        type: "OPEN_URL",
        data: {
          args: "https://vpn.mozilla.org/",
        },
      },
    },
  },
  groups: ["panel-test-provider", "pbNewtab"],
  targeting: "true",
  frequency: {
    lifetime: 3,
  },
  provider: "panel_local_testing",
};

add_task(async function test_show_private_browsing_message() {
  const messageSandbox = sinon.createSandbox();
  let { cleanup, browser } = await openMessagePreviewTab();
  let aboutMessagePreviewActor = await getAboutMessagePreviewParent(browser);
  messageSandbox.spy(aboutMessagePreviewActor, "showMessage");
  registerCleanupFunction(() => {
    messageSandbox.restore();
  });

  await aboutMessagePreviewActor.receiveMessage({
    name: "MessagePreview:SHOW_MESSAGE",
    data: JSON.stringify(TEST_PB_MESSAGE),
  });

  const { callCount } = aboutMessagePreviewActor.showMessage;
  Assert.greaterOrEqual(callCount, 1, "showMessage was called");
  // A new private window should open
  let privateWin = await BrowserTestUtils.waitForNewWindow({
    url: "about:privatebrowsing?debug",
  });
  Assert.ok(privateWin, "Private window opened");

  let tab = privateWin.gBrowser.selectedBrowser;
  //Test the message content
  await test_private_message_content(
    tab,
    "renders the private browsing message",
    //Expected selectors
    [
      "div.promo.below-search.promo-visible", // message wrapper
      "div.promo-image-large", // main image
      "h1#promo-header", // main title
      "p#private-browsing-promo-text", // message body
      "button.vpn-promo.primary", // primary button
    ]
  );
  //Remember to clean up the extra window first
  await BrowserTestUtils.closeWindow(privateWin);
  await cleanup();
});
