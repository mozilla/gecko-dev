/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let knownSettingPanes = {
  pictureInPictureToggleEnabled: "paneGeneral",
};

async function withSettingVisible(settingId, testFn) {
  let pane = knownSettingPanes[settingId];
  let prefs = await openPreferencesViaOpenPreferencesAPI(pane, {
    leaveOpen: true,
  });
  Assert.equal(prefs.selectedPane, pane, "General pane was selected");
  let win = gBrowser.contentWindow;
  let el = win.document.getElementById(settingId);
  if (el) {
    el.scrollIntoView();
  }
  await new Promise(r => requestAnimationFrame(r));
  await testFn(win, el);
  let tabClosed = BrowserTestUtils.waitForTabClosing(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await tabClosed;
}

const PIP_SETTING_ID = "pictureInPictureToggleEnabled";
const PIP_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.enabled";
const PIP_TOGGLE_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.enabled";

add_task(async function testPiPSettingHiddenByPref() {
  await withSettingVisible(PIP_SETTING_ID, async function (_, el) {
    ok(BrowserTestUtils.isVisible(el), "checkbox is visible");
  });

  await SpecialPowers.pushPrefEnv({
    set: [[PIP_TOGGLE_ENABLED_PREF, false]],
  });
  await withSettingVisible(PIP_SETTING_ID, async function (_, el) {
    ok(!el || BrowserTestUtils.isHidden(el), "checkbox is hidden");
  });
  await SpecialPowers.popPrefEnv();
});

add_task(async function testPiPSettingTelemetry() {
  await withSettingVisible(PIP_SETTING_ID, async function (win, el) {
    Services.fog.testResetFOG();
    ok(Services.prefs.getBoolPref(PIP_ENABLED_PREF), "PiP enabled by default");
    ok(el.checked, "Checked by default");
    EventUtils.synthesizeMouseAtCenter(el, {}, win);
    ok(!el.checked, "Unchecked after click");
    ok(!Services.prefs.getBoolPref(PIP_ENABLED_PREF), "PiP is now disabled");
    let events = Glean.pictureinpictureSettings.disableSettings.testGetValue();
    is(events.length, 1, "There was a Glean event");
    is(events[0].category, "pictureinpicture.settings", "Category is correct");
    is(events[0].name, "disable_settings", "Name is correct");
    Services.fog.testResetFOG();
    EventUtils.synthesizeMouseAtCenter(el, {}, win);
    events = Glean.pictureinpictureSettings.disableSettings.testGetValue();
    ok(!events, "There were no new Glean events when enabling");
  });
});
