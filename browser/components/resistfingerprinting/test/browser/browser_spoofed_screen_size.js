/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const DEFAULT_URL = TEST_PATH + "file_dummy.html";

const SCREEN_AVAIL_OFFSET = (() => {
  switch (AppConstants.platform) {
    case "macosx":
      return 76;
    case "win":
      return 48;
  }
  return 0;
})();

const OUTER_OFFSET = (() => {
  switch (AppConstants.platform) {
    case "linux":
    case "macosx":
      return [0, 85];
    case "win":
      return [16, 93];
  }
  return [0, 0];
})();

async function resizeWindow(aTab, aWidth, aHeight) {
  let [w, h] = await SpecialPowers.spawn(aTab.linkedBrowser, [], async () => {
    let win = content.wrappedJSObject;
    return [win.innerWidth, win.innerHeight];
  });
  let dx = aWidth - w;
  let dy = aHeight - h;
  if (dx == 0 && dy == 0) {
    return;
  }
  let { promise, resolve } = Promise.withResolvers();
  window.onresize = () => resolve();
  window.resizeBy(dx, dy);
  await promise;
}

async function measure(aTab) {
  return SpecialPowers.spawn(aTab.linkedBrowser, [], async () => {
    let win = content.wrappedJSObject;
    return [
      [win.innerWidth, win.innerHeight],
      [win.outerWidth, win.outerHeight],
      [win.screen.availWidth, win.screen.availHeight],
      [win.screen.width, win.screen.height],
    ];
  });
}

async function test_spoofed_size(aTab, aInner, aScreen) {
  // Resize to a small size first, to be sure we will always get a resize event.
  window.resizeTo(500, 500);
  await resizeWindow(aTab, aInner[0], aInner[1]);
  let [inner, outer, avail, screen] = await measure(aTab);

  if (inner[0] != aInner[0] || inner[1] != aInner[1]) {
    info(
      `Bailing out this test because we reached platform limits (wanted=[${aInner[0]} ${aInner[1]}], measured=[${inner[0]} ${inner[1]}]).`
    );
    return;
  }

  let expectedOuter = [inner[0] + OUTER_OFFSET[0], inner[1] + OUTER_OFFSET[1]];
  let expectedAvail = [screen[0], screen[1] - SCREEN_AVAIL_OFFSET];

  is(outer[0], expectedOuter[0], "We spoofed the outer width as expected.");
  is(outer[1], expectedOuter[1], "We spoofed the outer height as expected.");
  is(
    avail[0],
    expectedAvail[0],
    "We spoofed the avail screen width as expected."
  );
  is(
    avail[1],
    expectedAvail[1],
    "We spoofed the avail screen height as expected."
  );
  is(screen[0], aScreen[0], "We spoofed the screen width as expected.");
  is(screen[1], aScreen[1], "We spoofed the screen height as expected.");
}

async function test_no_spoofing_fullscreen(aTab) {
  let { promise: focusPromise, resolve } = Promise.withResolvers();
  SimpleTest.waitForFocus(resolve, aTab.linkedBrowser.ownerGlobal);
  await focusPromise;
  let fullscreenPromise = BrowserTestUtils.waitForContentEvent(
    aTab.linkedBrowser,
    "fullscreenchange"
  );
  await SpecialPowers.spawn(aTab.linkedBrowser, [], async () => {
    await content.document.body.requestFullscreen();
  });
  await fullscreenPromise;

  let [inner, outer, avail, screen] = await measure(aTab);
  is(inner[0], outer[0], "Inner and outer width are the same in fullscreen");
  is(inner[1], outer[1], "Inner and outer height are the same in fullscreen");
  is(inner[0], avail[0], "Inner and avail width are the same in fullscreen");
  is(inner[1], avail[1], "Inner and avail height are the same in fullscreen");
  is(inner[0], screen[0], "Inner and screen width are the same in fullscreen");
  is(inner[1], screen[1], "Inner and screen height are the same in fullscreen");
  await SpecialPowers.spawn(aTab.linkedBrowser, [], async () => {
    await content.document.exitFullscreen();
  });
}

async function test_no_spoofing_privileged(aTab) {
  let [contentInner, contentOuter, contentAvail, contentScreen] =
    await measure(aTab);
  let [inner, outer] = await SpecialPowers.spawn(
    aTab.linkedBrowser,
    [],
    async () => [
      [content.innerWidth, content.innerHeight],
      [content.outerWidth, content.outerHeight],
    ]
  );
  is(
    inner[0],
    contentInner[0],
    "Inner width has not been spoofed in the privileged page"
  );
  is(
    inner[1],
    contentInner[1],
    "Inner height has not been spoofed in the privileged page"
  );
  is(
    outer[0],
    contentOuter[0],
    "Outer width has not been spoofed in the privileged page"
  );
  is(
    outer[1],
    contentOuter[1],
    "Outer height has not been spoofed in the privileged page"
  );
  is(
    window.screen.availWidth,
    contentAvail[0],
    "Screen avail width has not been spoofed in the privileged page"
  );
  is(
    window.screen.availHeight,
    contentAvail[1],
    "Screen avail height has not been spoofed in the privileged page"
  );
  is(
    window.screen.width,
    contentScreen[0],
    "Screen width has not been spoofed in the privileged page"
  );
  is(
    window.screen.height,
    contentScreen[1],
    "Screen height has not been spoofed in the privileged page"
  );
}

add_task(async function do_tests() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.resistFingerprinting", true]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    DEFAULT_URL
  );

  await test_spoofed_size(tab, [1400, 700], [1920, 1080]);
  await test_spoofed_size(tab, [2000, 700], [3840, 2160]);
  await test_spoofed_size(tab, [2000, 700], [3840, 2160]);
  await test_spoofed_size(tab, [5000, 700], [7680, 4320]);
  await test_spoofed_size(tab, [8000, 700], [7680, 4320]);
  await test_spoofed_size(tab, [1400, 1100], [3840, 2160]);

  await test_no_spoofing_fullscreen(tab);
  BrowserTestUtils.removeTab(tab);

  tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:config"
  );
  await test_no_spoofing_privileged(tab);
  BrowserTestUtils.removeTab(tab);
});
