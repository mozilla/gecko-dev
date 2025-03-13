"use strict";

const TEST_URI =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "file_empty.html";
const gPermissionName = "autoplay-media";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.navigation.requireUserInteraction", true]],
  });
});

async function assertMenulist(entries) {
  // Wait for the session data to be flushed before continuing the test
  await new Promise(resolve =>
    SessionStore.getSessionHistory(gBrowser.selectedTab, resolve)
  );

  let backButton = document.getElementById("back-button");
  let contextMenu = document.getElementById("backForwardMenu");

  info("waiting for the history menu to open");

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(backButton, {
    type: "contextmenu",
    button: 2,
  });
  await popupShownPromise;

  info("history menu opened");

  let nodes = contextMenu.childNodes;

  is(
    nodes.length,
    entries.length,
    "Has the expected number of contextMenu entries"
  );

  for (let i = 0; i < entries.length; i++) {
    let node = nodes[i];
    is(
      node.getAttribute("uri"),
      entries[i],
      "contextMenu node has the correct uri"
    );
  }

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;
}

async function playVideoAndCheckForUserInteraction(aMuteVideo) {
  for (const mode of ["call play", "autoplay attribute"]) {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: TEST_URI,
      },
      async browser => {
        let args = {
          name: `${mode} causes BBI to ${!aMuteVideo ? "not" : ""} skip`,
          shouldPlay: true,
          mode,
          muted: aMuteVideo,
        };
        await loadAutoplayVideo(browser, args);
        await checkVideoDidPlay(browser, args);

        let loaded = BrowserTestUtils.waitForLocationChange(
          gBrowser,
          TEST_URI + "#1"
        );
        await SpecialPowers.spawn(browser, [], async () => {
          content.history.pushState(null, "", "#1");
        });
        await loaded;

        if (!aMuteVideo) {
          await assertMenulist([TEST_URI + "#1", TEST_URI]);
        } else {
          await assertMenulist([TEST_URI + "#1"]);
        }
      }
    );
  }
}

add_task(async function played_video_should_cause_userinteraction() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.ALLOWED]],
  });
  await playVideoAndCheckForUserInteraction(false);
});

add_task(async function muted_video_should_not_cause_userinteraction() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED]],
  });
  await playVideoAndCheckForUserInteraction(true);
});
