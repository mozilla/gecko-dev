/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

let { DownloadsTaskbar } = ChromeUtils.importESModule(
  "resource:///modules/DownloadsTaskbar.sys.mjs"
);

let gProgress = {};
function setProgressState(platform, id, state, current, maximum) {
  gProgress[id].state = state;
  gProgress[id].current = current;
  gProgress[id].maximum = maximum;
  gProgress[id].platform = platform;
}

let gMockWinTaskbar = null;
let gMockGtkTaskbar = null;
add_setup(function registerMock() {
  gMockWinTaskbar = MockRegistrar.register("@mozilla.org/windows-taskbar;1", {
    get available() {
      return true;
    },
    getTaskbarProgress(docShell) {
      gProgress[docShell.outerWindowID] = {};
      return {
        setProgressState: setProgressState.bind(
          null,
          "windows",
          docShell.outerWindowID
        ),
      };
    },
    QueryInterface: ChromeUtils.generateQI([Ci.nsIWinTaskbar]),
  });

  gMockGtkTaskbar = MockRegistrar.register(
    "@mozilla.org/widget/taskbarprogress/gtk;1",
    {
      setProgressState() {
        setProgressState("linux", this._currentWindowId, ...arguments);
      },
      setPrimaryWindow(docShell) {
        this._currentWindowId = docShell.outerWindowID;
        if (!(this._currentWindowId in gProgress)) {
          gProgress[this._currentWindowId] = {};
        }
      },
      QueryInterface: ChromeUtils.generateQI([Ci.nsIGtkTaskbarProgress]),
    }
  );
});

registerCleanupFunction(async function () {
  MockRegistrar.unregister(gMockWinTaskbar);
  MockRegistrar.unregister(gMockGtkTaskbar);
  await resetBetweenTests();
});

function isProgressEqualTo(progress, platform, kind, state, fraction) {
  is(
    progress.platform,
    platform,
    `${kind} taskbar progress platform is ${platform}`
  );
  is(progress.state, state, `${kind} taskbar progress state is ${state}`);

  if (fraction == 0) {
    // special-case since progress.maximum could also be zero
    is(progress.current, 0, `${kind} taskbar state is 0%`);
  } else {
    is(
      (progress.current / progress.maximum).toFixed(3),
      fraction.toFixed(3),
      `${kind} taskbar state is ${fraction * 100}%`
    );
  }
}

async function getDownloadsList({ isPrivate }) {
  return await Downloads.getList(
    isPrivate ? Downloads.PRIVATE : Downloads.PUBLIC
  );
}

async function addDownload({ isPrivate }) {
  let download = await Downloads.createDownload({
    source: {
      url: "x-test-url://nonsense.test/",
      isPrivate,
    },
    target: {
      path: gTestTargetFile.path,
    },
  });

  await (await getDownloadsList({ isPrivate })).add(download);
  return download;
}

async function removeDownload(download) {
  await (
    await getDownloadsList({ isPrivate: download.source.isPrivate })
  ).remove(download);
}

async function resetBetweenTests() {
  // Remove all DownloadsTaskbar instances.
  DownloadsTaskbar.resetBetweenTests();

  // Reset progress bar state.
  gProgress = {};
}

// Ensures that the download progress is correctly assigned to the window
// that the download applies to. For example, a private download should go
// to a private window.
async function testActiveAndInactive({
  isPrivateDownload,
  publicWindow,
  privateWindow,
  platform,
}) {
  await resetBetweenTests();

  if (publicWindow) {
    await DownloadsTaskbar.registerIndicator(publicWindow, platform);
  }

  if (privateWindow) {
    await DownloadsTaskbar.registerIndicator(privateWindow, platform);
  }

  // One bar will be 'active' and the other 'inactive'. If the download
  // is public, then public will be active, and vice versa.
  let activeWindow = isPrivateDownload ? privateWindow : publicWindow;
  let inactiveWindow = isPrivateDownload ? publicWindow : privateWindow;

  let activeProgress = gProgress[activeWindow.docShell.outerWindowID];
  let inactiveProgress = gProgress[inactiveWindow.docShell.outerWindowID];

  isProgressEqualTo(
    activeProgress,
    platform,
    "active",
    Ci.nsITaskbarProgress.STATE_NO_PROGRESS,
    0
  );
  isProgressEqualTo(
    inactiveProgress,
    platform,
    "inactive",
    Ci.nsITaskbarProgress.STATE_NO_PROGRESS,
    0
  );

  let download = await addDownload({ isPrivate: isPrivateDownload });
  isProgressEqualTo(
    activeProgress,
    platform,
    "active",
    Ci.nsITaskbarProgress.STATE_NO_PROGRESS,
    0
  );
  isProgressEqualTo(
    inactiveProgress,
    platform,
    "inactive",
    Ci.nsITaskbarProgress.STATE_NO_PROGRESS,
    0
  );

  download.stopped = false;
  download.hasProgress = true;
  download.onchange();
  isProgressEqualTo(
    activeProgress,
    platform,
    "active",
    Ci.nsITaskbarProgress.STATE_NO_PROGRESS,
    0
  );
  isProgressEqualTo(
    inactiveProgress,
    platform,
    "inactive",
    Ci.nsITaskbarProgress.STATE_NO_PROGRESS,
    0
  );

  for (let i = 0; i < 100; i++) {
    download.currentBytes = i;
    download.totalBytes = 100;
    download.onchange();
    isProgressEqualTo(
      activeProgress,
      platform,
      "active",
      Ci.nsITaskbarProgress.STATE_NORMAL,
      i / 100
    );
    isProgressEqualTo(
      inactiveProgress,
      platform,
      "inactive",
      Ci.nsITaskbarProgress.STATE_NO_PROGRESS,
      0
    );
  }

  removeDownload(download);
}

// Tests that a download is visible on the correct window.
//
// Linux (the GTK backend) only supports a single window to display progress
// on. As such, it can't use testActiveAndInactive; this is a simplified version
// of that test that checks whether _any_ progress is set.
async function testRepresentative({
  isPrivateDownload,
  representative,
  platform,
}) {
  await resetBetweenTests();

  await DownloadsTaskbar.registerIndicator(representative, platform);

  let progress = gProgress[representative.docShell.outerWindowID];

  isProgressEqualTo(
    progress,
    platform,
    "active",
    Ci.nsITaskbarProgress.STATE_NO_PROGRESS,
    0
  );

  let download = await addDownload({ isPrivate: isPrivateDownload });
  isProgressEqualTo(
    progress,
    platform,
    "active",
    Ci.nsITaskbarProgress.STATE_NO_PROGRESS,
    0
  );

  download.stopped = false;
  download.hasProgress = true;
  download.totalBytes = 100;
  download.onchange();
  isProgressEqualTo(
    progress,
    platform,
    "active",
    Ci.nsITaskbarProgress.STATE_NORMAL,
    0
  );

  for (let i = 0; i < 100; i++) {
    download.currentBytes = i;
    download.totalBytes = 100;
    download.onchange();
    isProgressEqualTo(
      progress,
      platform,
      "active",
      Ci.nsITaskbarProgress.STATE_NORMAL,
      i / 100
    );
  }

  await removeDownload(download);
}

add_task(async function test_downloadsTaskbar() {
  let publicWindow = window;
  let privateWindow = BrowserWindowTracker.openWindow({
    private: true,
  });

  if (AppConstants.platform == "win") {
    await testActiveAndInactive({
      isPrivateDownload: false,
      publicWindow,
      privateWindow,
      platform: "windows",
    });

    await testActiveAndInactive({
      isPrivateDownload: true,
      publicWindow,
      privateWindow,
      platform: "windows",
    });
  }

  if (AppConstants.platform == "linux") {
    await testRepresentative({
      isPrivateDownload: false,
      representative: publicWindow,
      platform: "linux",
    });

    await testRepresentative({
      isPrivateDownload: true,
      representative: publicWindow,
      platform: "linux",
    });
  }

  privateWindow.close();
});
