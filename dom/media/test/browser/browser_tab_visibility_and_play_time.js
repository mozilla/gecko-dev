/**
 * This test is used to ensure that invisible play time would be accumulated
 * when tab is in background. It also checks the HDR video accumulation time.
 * However, this test won't directly check the reported telemetry result,
 * because we can't check the snapshot histogram in the content process.
 * The actual probe checking happens in `test_accumulated_play_time.html`.
 */
'use strict';

const PAGE_URL =
    'https://example.com/browser/dom/media/test/browser/file_media.html';

add_task(async function testChangingTabVisibilityAffectsInvisiblePlayTime() {
  const originalTab = gBrowser.selectedTab;
  const mediaTab = await openMediaTab(PAGE_URL);

  info(`measuring play time when tab is in foreground`);
  await startMedia({
    mediaTab,
    shouldAccumulateTime: true,
    shouldAccumulateInvisibleTime: false,
    shouldAccumulateHDRTime: true,
  });
  await pauseMedia(mediaTab);

  info(`measuring play time when tab is in background`);
  await BrowserTestUtils.switchTab(window.gBrowser, originalTab);
  await startMedia({
    mediaTab,
    shouldAccumulateTime: true,
    shouldAccumulateInvisibleTime: true,
    shouldAccumulateHDRTime: true,
  });
  await pauseMedia(mediaTab);

  BrowserTestUtils.removeTab(mediaTab);
});

/**
 * Following are helper functions.
 */
async function openMediaTab(url) {
  info(`open tab for media playback`);
  const tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser, url);
  info(`add content helper functions and variables`);
  await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    content.waitForOnTimeUpdate = element => {
      return new Promise(resolve => {
        // Wait longer to ensure the system clock has actually moved forward,
        // preventing intermittent failures.
        let count = 0;
        const listener = () => {
          if (++count > 2) {
            element.removeEventListener('timeupdate', listener);
            resolve();
          }
        };
        element.addEventListener('timeupdate', listener);
      });
    };

    content.sleep = ms => {
      return new Promise(resolve => content.setTimeout(resolve, ms));
    };

    content.assertAttributeDefined = (videoChrome, checkType) => {
      Assert.notEqual(videoChrome[checkType], undefined, `${checkType} exists`);
    };
    content.assertValueEqualTo = (videoChrome, checkType, expectedValue) => {
      content.assertAttributeDefined(videoChrome, checkType);
      is(videoChrome[checkType], expectedValue,
         `${checkType} equals to ${expectedValue}`

      );
    };
    content.assertValueConstantlyIncreases = async (videoChrome, checkType) => {
      content.assertAttributeDefined(videoChrome, checkType);
      const valueSnapshot = videoChrome[checkType];
      await content.waitForOnTimeUpdate(videoChrome);
      Assert.greater(
          videoChrome[checkType], valueSnapshot,
          `${checkType} keeps increasing`);
    };
    content.assertValueKeptUnchanged = async (videoChrome, checkType) => {
      content.assertAttributeDefined(videoChrome, checkType);
      const valueSnapshot = videoChrome[checkType];
      await content.sleep(1000);
      Assert.equal(
          videoChrome[checkType], valueSnapshot,
          `${checkType} keeps unchanged`);
    };
  });
  return tab;
}

function startMedia({
  mediaTab,
  shouldAccumulateTime,
  shouldAccumulateInvisibleTime,
  shouldAccumulateHDRTime,
}) {
  return SpecialPowers.spawn(
      mediaTab.linkedBrowser,
      [
        shouldAccumulateTime,
        shouldAccumulateInvisibleTime,
        shouldAccumulateHDRTime,
      ],
      async (accumulateTime, accumulateInvisibleTime, accumulateHDRTime) => {
        const video = content.document.getElementById('video');
        ok(await video.play().then(() => true, () => false),
           'video started playing');
        const videoChrome = SpecialPowers.wrap(video);
        if (accumulateTime) {
          await content.assertValueConstantlyIncreases(
              videoChrome, 'totalVideoPlayTime');
        } else {
          await content.assertValueKeptUnchanged(
              videoChrome, 'totalVideoPlayTime');
        }
        if (accumulateInvisibleTime) {
          await content.assertValueConstantlyIncreases(
              videoChrome, 'invisiblePlayTime');
        } else {
          await content.assertValueKeptUnchanged(
              videoChrome, 'invisiblePlayTime');
        }

        const videoHDR = content.document.getElementById('videoHDR');
        ok(videoHDR.play().then(() => true, () => false),
           'videoHDR started playing');
        const videoHDRChrome = SpecialPowers.wrap(videoHDR);
        if (accumulateHDRTime) {
          await content.assertValueConstantlyIncreases(
              videoHDRChrome, 'totalVideoHDRPlayTime');
        } else {
          await content.assertValueKeptUnchanged(
              videoHDRChrome, 'totalVideoHDRPlayTime');
        }
      });
}

function pauseMedia(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    const video = content.document.getElementById('video');
    video.pause();
    ok(true, 'video paused');
    const videoChrome = SpecialPowers.wrap(video);
    await content.assertValueKeptUnchanged(videoChrome, 'totalVideoPlayTime');
    await content.assertValueKeptUnchanged(videoChrome, 'invisiblePlayTime');

    const videoHDR = content.document.getElementById('videoHDR');
    videoHDR.pause();
    ok(true, 'videoHDR paused');
    const videoHDRChrome = SpecialPowers.wrap(videoHDR);
    await content.assertValueKeptUnchanged(
        videoHDRChrome, 'totalVideoHDRPlayTime');
  });
}
