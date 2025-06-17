/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE = `
<!doctype html>
<style>
  :root::view-transition {
    background-color: pink;
  }
  :root::view-transition-group(*) {
    /* Keeps the transition up until timeout */
    animation-play-state: paused;
    opacity: 0;
  }
</style>
`;

async function detachTab(tab) {
  let newWindowPromise = BrowserTestUtils.waitForNewWindow();
  await EventUtils.synthesizePlainDragAndDrop({
    srcElement: tab,
    // destElement is null because tab detaching happens due
    // to a drag'n'drop on an invalid drop target.
    destElement: null,
    // don't move horizontally because that could cause a tab move
    // animation, and there's code to prevent a tab detaching if
    // the dragged tab is released while the animation is running.
    stepX: 0,
    stepY: 100,
  });
  return newWindowPromise;
}

// Test for bug 1972259
add_task(async function test_tab_detach_active_vt() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: `https://example.com/document-builder.sjs?html=${encodeURIComponent(
        TEST_PAGE
      )}`,
    },
    async browser => {
      await ContentTask.spawn(browser, null, async function () {
        return content.document.startViewTransition().ready;
      });
      let newWin = await detachTab(gBrowser.selectedTab);
      ok(!!newWin, "Opened a new window, without crashing");
      await BrowserTestUtils.closeWindow(newWin);
    }
  );
});
