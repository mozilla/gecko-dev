/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Tests that when Report Broken Site is disabled, it will
 * send the user to webcompat.com when clicked and it the
 * relevant tab's report data.
 */

/* import-globals-from send_more_info.js */

"use strict";

Services.scriptloader.loadSubScript(
  getRootDirectory(gTestPath) + "send_more_info.js",
  this
);

add_common_setup();

const VIDEO_URL = `${BASE_URL}/videotest.mp4`;

add_task(async function testWebcompatComFallbacks() {
  ensureReportBrokenSitePreffedOff();

  const tab = await openTab(REPORTABLE_PAGE_URL);

  await testWebcompatComFallback(tab, AppMenu());

  await changeTab(tab, REPORTABLE_PAGE_URL2);
  await testWebcompatComFallback(tab, ProtectionsPanel());

  // also load a video to ensure system codec
  // information is loaded and properly sent
  const tab2 = await openTab(VIDEO_URL);
  await testWebcompatComFallback(tab2, HelpMenu());
  closeTab(tab2);

  closeTab(tab);
});

async function testMenuDisabledForInvalidURLs(menu) {
  ensureReportBrokenSitePreffedOff();

  await menu.open();
  menu.isReportBrokenSiteDisabled();
  await menu.close();
}

async function testMenuEnabledForValidURLs(menu) {
  ensureReportBrokenSitePreffedOff();

  await BrowserTestUtils.withNewTab(REPORTABLE_PAGE_URL, async function () {
    await menu.open();
    menu.isReportBrokenSiteEnabled();
    await menu.close();
  });
}

add_task(async function testDisabledForInvalidURLsHelpMenu() {
  await testMenuDisabledForInvalidURLs(AppMenu());
  await testMenuDisabledForInvalidURLs(HelpMenu());
  await testMenuDisabledForInvalidURLs(ProtectionsPanel());
});

add_task(async function testEnabledForValidURLs() {
  await testMenuEnabledForValidURLs(AppMenu());
  await testMenuEnabledForValidURLs(HelpMenu());
  await testMenuEnabledForValidURLs(ProtectionsPanel());
});
