/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_textSelectedDuringScreenshot() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SELECTION_TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      let rect = await helper.getTestPageElementRect("selection");

      await ContentTask.spawn(browser, [], async () => {
        let selection = content.window.getSelection();
        let elToSelect = content.document.getElementById("selection");

        let range = content.document.createRange();
        range.selectNode(elToSelect);
        selection.addRange(range);
      });

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      await helper.clickTestPageElement("selection");

      // The selection doesn't get cleared in the tests so just manually
      // remove the selection here.
      // In real scenarios, the selection is cleared when the page is
      // interacted with.
      await ContentTask.spawn(browser, [], async () => {
        let selection = content.window.getSelection();
        selection.removeAllRanges();
      });

      let clipboardChanged = helper.waitForRawClipboardChange(
        Math.round(rect.width),
        Math.round(rect.height),
        { allPixels: true }
      );

      await helper.clickCopyButton();

      info("Waiting for clipboard change");
      let result = await clipboardChanged;
      let allPixels = result.allPixels;
      info(`${typeof allPixels}, ${allPixels.length}`);

      let sumOfPixels = Object.values(allPixels).reduce(
        (accumulator, currentVal) => accumulator + currentVal,
        0
      );

      Assert.less(
        sumOfPixels,
        allPixels.length * 255,
        "Sum of pixels is less than all white pixels"
      );
    }
  );
});

add_task(async function test_textSelectedDuringVisibleScreenshot() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SELECTION_TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      let devicePixelRatio = await getContentDevicePixelRatio(browser);

      let expectedWidth = Math.floor(
        devicePixelRatio * contentInfo.clientWidth
      );
      let expectedHeight = Math.floor(
        devicePixelRatio * contentInfo.clientHeight
      );

      await ContentTask.spawn(browser, [], async () => {
        let selection = content.window.getSelection();
        let elToSelect = content.document.getElementById("selection");

        let range = content.document.createRange();
        range.selectNode(elToSelect);
        selection.addRange(range);
      });

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );

      let panel = gBrowser.selectedBrowser.ownerDocument.querySelector(
        "#screenshotsPagePanel"
      );

      // click the visible page button in panel
      let visiblePageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector("#visible-page");
      visiblePageButton.click();

      await screenshotReady;

      let copyButton = helper.getDialogButton("copy");
      ok(copyButton, "Got the copy button");

      let clipboardChanged = helper.waitForRawClipboardChange(
        expectedWidth,
        expectedHeight,
        { allPixels: true }
      );

      copyButton.click();

      info("Waiting for clipboard change");
      let result = await clipboardChanged;
      let allPixels = result.allPixels;
      info(`${typeof allPixels}, ${allPixels.length}`);

      let sumOfPixels = Object.values(allPixels).reduce(
        (accumulator, currentVal) => accumulator + currentVal,
        0
      );

      Assert.less(
        sumOfPixels,
        allPixels.length * 255,
        "Sum of pixels is less than all white pixels"
      );
    }
  );
});
