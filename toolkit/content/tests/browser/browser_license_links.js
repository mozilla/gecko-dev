/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify that we can reach about:buildconfig using links
 * from about:license.
 */
add_task(async function check_links() {
  await BrowserTestUtils.withNewTab("about:license", async browser => {
    let otherPage = "about:buildconfig";
    let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, otherPage);
    await BrowserTestUtils.synthesizeMouse(
      `a[href='${otherPage}']`,
      2,
      2,
      { accelKey: true },
      browser
    );
    info("Clicked " + otherPage + " link");
    let tab = await tabPromise;
    ok(true, otherPage + " tab opened correctly");
    BrowserTestUtils.removeTab(tab);
  });
});
