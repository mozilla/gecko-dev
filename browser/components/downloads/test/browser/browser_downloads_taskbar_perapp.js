/* Any copyright is dedicated to the Public Domain.
 *   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

let { DownloadsTaskbar } = ChromeUtils.importESModule(
  "resource:///modules/DownloadsTaskbar.sys.mjs"
);

let gMockMacTaskbar = null;
let gProgress = {};
add_setup(function registerMock() {
  gMockMacTaskbar = MockRegistrar.register(
    "@mozilla.org/widget/macdocksupport;1",
    {
      setProgressState(state, current, maximum) {
        gProgress.state = state;
        gProgress.current = current;
        gProgress.maximum = maximum;
      },
      QueryInterface: ChromeUtils.generateQI([Ci.nsITaskbarProgress]),
    }
  );
});

registerCleanupFunction(async function () {
  MockRegistrar.unregister(gMockMacTaskbar);
  await resetBetweenTests();
});

function isProgressEqualTo(progress, state, fraction) {
  is(progress.state, state);

  if (fraction == 0) {
    // special-case since progress.maximum could also be zero
    is(progress.current, 0);
  } else {
    is(
      (progress.current / progress.maximum).toFixed(3),
      fraction.toFixed(3),
      `app taskbar state is ${fraction * 100}%`
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

add_task(async function test_downloadsTaskbar() {
  await resetBetweenTests();
  await DownloadsTaskbar.registerIndicator(
    window,
    AppConstants.platform == "mac" ? null : "mac"
  );

  let privateDownload = await addDownload({ isPrivate: true });
  let publicDownload = await addDownload({ isPrivate: false });
  isProgressEqualTo(gProgress, Ci.nsITaskbarProgress.STATE_NO_PROGRESS, 0);

  privateDownload.stopped = false;
  privateDownload.hasProgress = true;
  privateDownload.totalBytes = 100;
  privateDownload.onchange();
  isProgressEqualTo(gProgress, Ci.nsITaskbarProgress.STATE_NORMAL, 0);

  for (let i = 1; i <= 50; i++) {
    privateDownload.currentBytes = i;
    privateDownload.onchange();
    isProgressEqualTo(gProgress, Ci.nsITaskbarProgress.STATE_NORMAL, i / 100);
  }

  publicDownload.stopped = false;
  publicDownload.hasProgress = true;
  publicDownload.totalBytes = 100;
  publicDownload.onchange();
  isProgressEqualTo(gProgress, Ci.nsITaskbarProgress.STATE_NORMAL, 50 / 200);

  for (let i = 1; i <= 100; i++) {
    publicDownload.currentBytes = i;
    publicDownload.onchange();
    isProgressEqualTo(
      gProgress,
      Ci.nsITaskbarProgress.STATE_NORMAL,
      (i + 50) / 200
    );
  }

  for (let i = 51; i < 100; i++) {
    privateDownload.currentBytes = i;
    privateDownload.onchange();
    isProgressEqualTo(
      gProgress,
      Ci.nsITaskbarProgress.STATE_NORMAL,
      (i + 100) / 200
    );
  }

  removeDownload(privateDownload);
  removeDownload(publicDownload);
  await resetBetweenTests();
});
