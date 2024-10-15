/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const MIN_DURATION_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.min-video-secs";
const ALWAYS_SHOW_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.always-show";

add_setup(async () => {
  const PIP_ON_TAB_SWITCH_ENABLED_PREF =
    "media.videocontrols.picture-in-picture.enable-when-switching-tabs.enabled";

  await SpecialPowers.pushPrefEnv({
    set: [[PIP_ON_TAB_SWITCH_ENABLED_PREF, true]],
  });
});

/**
 * Tests that the PiP window is automatically opened and closed when switching
 * between tabs, ensuring that the main browser window maintains focus.
 */
add_task(async function autopip_and_focus() {
  // Open a new window and save a handle for the first tab
  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  let firstTab = win1.gBrowser.selectedTab;

  // Open a new tab containing a video
  let pipTab = await BrowserTestUtils.openNewForegroundTab(
    win1.gBrowser,
    TEST_PAGE
  );
  let browser = pipTab.linkedBrowser;
  let secondTab = win1.gBrowser.selectedTab;

  // Ensure the video is playing
  let videoID = "with-controls";
  await ensureVideosReady(browser);
  await SpecialPowers.spawn(browser, [videoID], async videoID => {
    await content.document.getElementById(videoID).play();
  });

  // We will check if the PiP window will automatically open on tab switch
  let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);

  // Also, we will check that the main window does not lose focus when the PiP
  // window opens
  let blurAborter = new AbortController();
  win1.addEventListener(
    "blur",
    () => {
      ok(false, "Should never have seen the blur event!");
    },
    { signal: blurAborter.signal }
  );

  // Switch from the video tab to the first tab and check if the PiP window opened
  await BrowserTestUtils.switchTab(win1.gBrowser, firstTab);
  let pipWin = await domWindowOpened;
  ok(pipWin, "PiP window automatically opened.");

  // Check if the main window lost its focus or if timeout has been triggered
  await new Promise(resolve => pipWin.requestAnimationFrame(resolve));

  is(Services.focus.activeWindow, win1, "First window is still focused");
  blurAborter.abort();

  // Switch back to the video tab and check if the PiP window closes
  let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
  await BrowserTestUtils.switchTab(win1.gBrowser, secondTab);
  ok(await pipClosed, "PiP window automatically closed.");

  await BrowserTestUtils.closeWindow(win1);
});

/**
 * Tests that the manual and automatic PiP triggers do not interfere with each other:
 *  - If a user manually opens PiP and switches tab, it is not closed automatically
 *  - If a user manually closes PiP and switches tab, the automaticatic trigger still works
 */
add_task(async function autopip_interference() {
  // Save a handle for the first tab
  let firstTab = gBrowser.selectedTab;
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
      waitForLoad: true,
    },
    async browser => {
      // Ensure the video is playing
      let videoID = "with-controls";
      await ensureVideosReady(browser);
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        await content.document.getElementById(videoID).play();
      });

      // Simulate user manually triggering PiP
      let pipWin = await triggerPictureInPicture(browser, "with-controls");
      ok(pipWin, "PiP window has been manually opened by the user");

      // Switch onto another tab
      let secondTab = gBrowser.selectedTab;
      await BrowserTestUtils.switchTab(gBrowser, firstTab);

      // We will check if the PiP window remains still open, waiting for 2 seconds
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => setTimeout(resolve, 2000));
      ok(!pipWin.closed, "PiP window should still be open.");

      // Switch back to the video tab and check if the PiP window is still open
      await BrowserTestUtils.switchTab(gBrowser, secondTab);

      // Simulate user manually closing the PiP window
      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let closeButton = pipWin.document.getElementById("close");
      EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
      ok(await pipClosed, "PiP window has been manually closed by the user");

      // Play again the video (closing the PiP window causes it to pause)
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        await content.document.getElementById(videoID).play();
      });

      // Switch to the other tab again
      let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);
      await BrowserTestUtils.switchTab(gBrowser, firstTab);

      // Check that the PiP window has automatically opened
      let pipWinAuto = await domWindowOpened;
      ok(pipWinAuto, "PiP window automatically opened.");

      // Switch back to the video tab and check that the PiP window has automatically closed
      let pipClosedAuto = BrowserTestUtils.domWindowClosed(pipWinAuto);
      await BrowserTestUtils.switchTab(gBrowser, secondTab);
      ok(await pipClosedAuto, "PiP window automatically closed.");
    }
  );
});

/**
 * Tests that silent videos do not qualify for auto-PiP.
 */
add_task(async function autopip_silent_videos() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [ALWAYS_SHOW_PREF, false],
      [MIN_DURATION_PREF, 3], // The sample video is 4 seconds long.
    ],
  });

  let firstTab = gBrowser.selectedTab;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE_WITHOUT_AUDIO,
    },
    async browser => {
      // Ensure the video is playing
      let videoID = "without-audio";
      await ensureVideosReady(browser);
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        await content.document.getElementById(videoID).play();
      });

      let visibilityChange = BrowserTestUtils.waitForContentEvent(
        browser,
        "visibilitychange"
      );
      await BrowserTestUtils.switchTab(gBrowser, firstTab);
      await visibilityChange;

      // Wait a few seconds for the event loop to have sent messages down to
      // autoPiP.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => setTimeout(resolve, 2000));
      assertNoPiPWindowsOpen();
    }
  );
});

/**
 * Tests that small videos do not qualify for auto-PiP, even if they have sound.
 */
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      [ALWAYS_SHOW_PREF, false],
      [MIN_DURATION_PREF, 3], // The sample video is 5 seconds long.
    ],
  });

  let firstTab = gBrowser.selectedTab;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE_WITH_SOUND,
    },
    async browser => {
      // Ensure the video is playing
      let videoID = "with-controls";
      await ensureVideosReady(browser);
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        // Force the video to be small.
        let videoEl = content.document.getElementById(videoID);
        videoEl.style.maxWidth = "10px";
        videoEl.style.maxHeight = "10px";
        await videoEl.play();
      });

      let visibilityChange = BrowserTestUtils.waitForContentEvent(
        browser,
        "visibilitychange"
      );
      await BrowserTestUtils.switchTab(gBrowser, firstTab);
      await visibilityChange;

      // Wait a few seconds for the event loop to have sent messages down to
      // autoPiP.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => setTimeout(resolve, 2000));
      assertNoPiPWindowsOpen();
    }
  );
});
