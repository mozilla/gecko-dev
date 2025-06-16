/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function mainTest() {
  await task_resetState();

  let verdicts = [
    Downloads.Error.BLOCK_VERDICT_UNCOMMON,
    Downloads.Error.BLOCK_VERDICT_MALWARE,
    Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED,
    Downloads.Error.BLOCK_VERDICT_INSECURE,
  ];
  let downloads = [];
  verdicts.map(v => makeDownload(v)).forEach(d => downloads.push(d));
  downloads.push(makeContentAnalysisWarnDownload());
  await task_addDownloads(downloads);

  // Check that the richlistitem for each download is correct.
  for (let i = 0; i < downloads.length; i++) {
    await task_openPanel();

    // Handle items backwards, using lastElementChild, to ensure there's no
    // code wrongly resetting the selection to the first item during the process.
    let item = DownloadsView.richListBox.lastElementChild;

    info("Open the panel and click the item to show the subview.");
    let viewPromise = promiseViewShown(DownloadsBlockedSubview.subview);
    EventUtils.synthesizeMouseAtCenter(item, {});
    await viewPromise;

    // Items are listed in newest-to-oldest order, so e.g. the first item's
    // verdict is the last element in the verdicts array.
    // But we're handling items backwards per above, so iterating forwards
    // gives us the expected order.
    let currentDownload = downloads[i];
    is(
      DownloadsBlockedSubview.subview.getAttribute("verdict"),
      currentDownload.errorObj.reputationCheckVerdict
    );

    info("Go back to the main view.");
    viewPromise = promiseViewShown(DownloadsBlockedSubview.mainView);
    DownloadsBlockedSubview.panelMultiView.goBack();
    await viewPromise;

    info("Show the subview again.");
    viewPromise = promiseViewShown(DownloadsBlockedSubview.subview);
    EventUtils.synthesizeMouseAtCenter(item, {});
    await viewPromise;

    ok(!DownloadsBlockedSubview.elements.unblockButton.hidden);
    ok(!DownloadsBlockedSubview.elements.deleteButton.hidden);
    info("Click the Open button.");
    // The download should be unblocked and then opened,
    // i.e., unblockAndOpenDownload() should be called on the item.  The panel
    // should also be closed as a result, so wait for that too.
    let unblockPromise = promiseUnblockAndSaveCalled(item);
    let hidePromise = promisePanelHidden();
    // Simulate a mousemove to ensure it's not wrongly being handled by the
    // panel as the user changing download selection.
    EventUtils.synthesizeMouseAtCenter(
      DownloadsBlockedSubview.elements.unblockButton,
      { type: "mousemove" }
    );
    EventUtils.synthesizeMouseAtCenter(
      DownloadsBlockedSubview.elements.unblockButton,
      {}
    );
    info("waiting for unblockOpen");
    await unblockPromise;
    info("waiting for hide panel");
    await hidePromise;

    window.focus();
    await SimpleTest.promiseFocus(window);

    info("Reopen the panel and show the subview again.");
    await task_openPanel();
    viewPromise = promiseViewShown(DownloadsBlockedSubview.subview);
    EventUtils.synthesizeMouseAtCenter(item, {});
    await viewPromise;

    info("Click the Remove button.");
    // The panel should close and the item should be removed from it.
    hidePromise = promisePanelHidden();
    EventUtils.synthesizeMouseAtCenter(
      DownloadsBlockedSubview.elements.deleteButton,
      {}
    );
    info("Waiting for hide panel");
    await hidePromise;

    info("Open the panel again and check the item is gone.");
    await task_openPanel();
    Assert.ok(!item.parentNode);

    hidePromise = promisePanelHidden();
    DownloadsPanel.hidePanel();
    await hidePromise;
  }

  await task_resetState();
});

add_task(async function test_content_analysis_blocked_file() {
  await task_resetState();

  await task_addDownloads([makeContentAnalysisBlockedDownload()]);

  // Check that the richlistitem for each download is correct.
  await task_openPanel();

  // Handle items backwards, using lastElementChild, to ensure there's no
  // code wrongly resetting the selection to the first item during the process.
  let item = DownloadsView.richListBox.lastElementChild;

  info("Open the panel and click the item to show the subview.");
  let viewPromise = promiseViewShown(DownloadsBlockedSubview.subview);
  EventUtils.synthesizeMouseAtCenter(item, {});
  await viewPromise;

  ok(DownloadsBlockedSubview.elements.unblockButton.hidden);
  ok(DownloadsBlockedSubview.elements.deleteButton.hidden);

  await task_resetState();
});

function promisePanelHidden() {
  return BrowserTestUtils.waitForEvent(DownloadsPanel.panel, "popuphidden");
}

function makeDownload(verdict) {
  return {
    state: DownloadsCommon.DOWNLOAD_DIRTY,
    hasBlockedData: true,
    errorObj: {
      result: Cr.NS_ERROR_FAILURE,
      message: "Download blocked.",
      becauseBlocked: true,
      becauseBlockedByContentAnalysis: false,
      becauseBlockedByReputationCheck: true,
      reputationCheckVerdict: verdict,
    },
  };
}

function makeContentAnalysisBlockedDownload() {
  return {
    state: DownloadsCommon.DOWNLOAD_DIRTY,
    hasBlockedData: true,
    errorObj: {
      result: Cr.NS_ERROR_FAILURE,
      message: "Download blocked.",
      becauseBlocked: true,
      becauseBlockedByContentAnalysis: true,
      becauseBlockedByReputationCheck: false,
      reputationCheckVerdict: Downloads.Error.BLOCK_VERDICT_MALWARE,
    },
  };
}

function makeContentAnalysisWarnDownload() {
  return {
    state: DownloadsCommon.DOWNLOAD_DIRTY,
    hasBlockedData: true,
    errorObj: {
      result: Cr.NS_ERROR_FAILURE,
      message: "Download warned.",
      becauseBlocked: true,
      becauseBlockedByContentAnalysis: true,
      becauseBlockedByReputationCheck: false,
      reputationCheckVerdict:
        Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED,
    },
  };
}

function promiseViewShown(view) {
  return BrowserTestUtils.waitForEvent(view, "ViewShown");
}

function promiseUnblockAndSaveCalled(item) {
  return new Promise(resolve => {
    let realFn = item._shell.unblockAndSave;
    item._shell.unblockAndSave = async () => {
      item._shell.unblockAndSave = realFn;
      resolve();
    };
  });
}
