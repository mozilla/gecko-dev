const PAGE_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_autoplay.html";

const testVideoId = "video";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

/**
 * This test is used to check if the `seekto` action can be sent when calling
 * media controller's `seekTo()`. In addition, the seeking related properties
 * which would be sent to the action handler should also be correct as what we
 * set in `seekTo()`.
 */
add_task(async function testSetPositionState() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_URL);

  info(`start media`);
  await playMedia(tab, testVideoId);

  const seektime = 0;
  info(`seek to ${seektime} seconds.`);
  await PerformSeekTo(tab, {
    seekTime: seektime,
  });

  info(`seek to ${seektime} seconds and set fastseek to true`);
  await PerformSeekTo(tab, {
    seekTime: seektime,
    fastSeek: true,
  });

  info(`seek to ${seektime} seconds and set fastseek to false`);
  await PerformSeekTo(tab, {
    seekTime: seektime,
    fastSeek: false,
  });

  info(`remove tab`);
  await tab.close();
});

add_task(async function testSeekRelative() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_URL);

  info(`start media`);
  await playMedia(tab, testVideoId);

  let seekoffset = 5;
  info(`seek forward ${seekoffset} seconds`);
  await PerformSeekRelative(tab, "seekforward", {
    seekOffset: seekoffset,
  });

  seekoffset = 4;
  info(`seek backward ${seekoffset} seconds`);
  await PerformSeekRelative(tab, "seekbackward", {
    seekOffset: seekoffset,
  });

  info(`remove tab`);
  await tab.close();
});

/**
 * The following is helper function.
 */
async function PerformSeekTo(tab, seekDetails) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [seekDetails, testVideoId],
    (seekDetails, Id) => {
      const { seekTime, fastSeek } = seekDetails;
      content.navigator.mediaSession.setActionHandler("seekto", details => {
        Assert.notEqual(
          details.seekTime,
          undefined,
          "Seektime must be presented"
        );
        is(seekTime, details.seekTime, "Get correct seektime");
        if (fastSeek) {
          is(fastSeek, details.fastSeek, "Get correct fastSeek");
        } else {
          Assert.strictEqual(
            details.fastSeek,
            undefined,
            "Details should not contain fastSeek"
          );
        }
        // We use `onseek` as a hint to know if the `seekto` has been received
        // or not. The reason we don't return a resolved promise instead is
        // because if we do so, it can't guarantees that the `seekto` action
        // handler has been set before calling `mediaController.seekTo`.
        content.document.getElementById(Id).currentTime = seekTime;
      });
    }
  );
  const seekPromise = SpecialPowers.spawn(
    tab.linkedBrowser,
    [testVideoId],
    Id => {
      const video = content.document.getElementById(Id);
      return new Promise(r => (video.onseeked = () => r()));
    }
  );
  const { seekTime, fastSeek } = seekDetails;
  tab.linkedBrowser.browsingContext.mediaController.seekTo(seekTime, fastSeek);
  await seekPromise;
}

async function PerformSeekRelative(tab, mode, seekDetails) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [seekDetails, mode, testVideoId],
    (seekDetails, mode, Id) => {
      const { seekOffset } = seekDetails;
      content.navigator.mediaSession.setActionHandler(mode, details => {
        Assert.notEqual(
          details.seekOffset,
          undefined,
          "Seekoffset must be presented"
        );
        is(seekOffset, details.seekOffset, "Get correct seekoffset");

        content.document.getElementById(Id).currentTime +=
          mode == "seekforward" ? seekOffset : -seekOffset;
      });
    }
  );
  const seekPromise = SpecialPowers.spawn(
    tab.linkedBrowser,
    [testVideoId],
    Id => {
      const video = content.document.getElementById(Id);
      return new Promise(r => (video.onseeked = () => r()));
    }
  );
  const { seekOffset } = seekDetails;
  if (mode === "seekforward") {
    tab.linkedBrowser.browsingContext.mediaController.seekForward(seekOffset);
  } else {
    tab.linkedBrowser.browsingContext.mediaController.seekBackward(seekOffset);
  }
  await seekPromise;
}
