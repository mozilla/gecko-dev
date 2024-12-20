/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

const { ToolbarBadgeHub } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ToolbarBadgeHub.sys.mjs"
);

const DEFAULT_ICON = "avatar-empty.svg";
const HUMAN_CIRCLE_BADGED = "avatar-empty.svg";
const HUMAN_CIRCLE = "avatar-empty-circle.svg";
const FOX_CIRCLE_BADGED = "avatar-fox.svg";
const FOX_CIRCLE = "avatar-fox-circle.svg";

// No matter which branch we're in, we expect this icon to be used for the
// signed-in state by default.
const SIGNED_IN_ICON = "avatar.svg";

/**
 * Checks that the current icon for the FxA avatar menu button matches
 * iconFilename.
 *
 * @param {DOMWindow} win
 *   The browser window to test the icon for.
 * @param {string} iconFilename
 *   The expected iconFilename. This is not the full path to the image.
 *   Example: "avatar-empty.svg".
 * @param {string} message
 *   The assertion message to display.
 */
function assertCurrentIcon(win, iconFilename, message) {
  let image = win.document.querySelector("#fxa-avatar-image");
  let avatarURL = win.getComputedStyle(image).listStyleImage;
  let expectedURL = `url("chrome://browser/skin/fxa/${iconFilename}")`;
  Assert.equal(avatarURL, expectedURL, message);
}

/**
 * Asserts that we're in the signed-out state, and that the signed-out icon
 * in win matches our iconFilename.
 *
 * @param {DOMWindow} win
 *   The browser window to test the icon for.
 * @param {string} iconFilename
 *   The expected iconFilename. This is not the full path to the image.
 *   Example: "avatar-empty.svg".
 */
function assertSignedOutIcon(win, iconFilename) {
  Assert.equal(
    UIState.get().status,
    UIState.STATUS_NOT_CONFIGURED,
    "Currently signed out."
  );
  assertCurrentIcon(
    win,
    iconFilename,
    `Signed-out avatar image is ${iconFilename}`
  );
}

/**
 * Fakes out a signed-in state and then asserts that the signed-in icon
 * is the generic signed-in icon that we always use. Then we revert the
 * faked signed-in state.
 *
 * @param {DOMWindow} win
 *   The browser window to test the icon for.
 */
function assertSignedInIcon(win) {
  const oldUIState = UIState.get;

  UIState.get = () => ({
    status: UIState.STATUS_SIGNED_IN,
    lastSync: new Date(),
    email: "foo@bar.com",
  });
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  assertCurrentIcon(
    win,
    SIGNED_IN_ICON,
    `Signed-in avatar image is ${SIGNED_IN_ICON}`
  );

  UIState.get = oldUIState;
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
}

/**
 * Asserts that we're in the signed-out state, and that when badged, we're
 * showing the iconFilename icon in the window. This reverts any badging state
 * on the window before returning.
 *
 * @param {DOMWindow} win
 *   The browser window to test the icon for.
 * @param {string} iconFilename
 *   The expected iconFilename. This is not the full path to the image.
 *   Example: "avatar-empty.svg".
 */
function assertBadgedIcon(win, iconFilename) {
  Assert.equal(
    UIState.get().status,
    UIState.STATUS_NOT_CONFIGURED,
    "Currently signed out."
  );

  let button = win.document.querySelector("#fxa-toolbar-menu-button");
  let badge = button.querySelector(".toolbarbutton-badge");
  badge.classList.add("feature-callout");
  button.setAttribute("badged", true);
  button.toggleAttribute("showing-callout", true);

  assertCurrentIcon(
    win,
    iconFilename,
    `Badged avatar image is ${iconFilename}`
  );

  badge.classList.remove("feature-callout");
  button.removeAttribute("badged");
  button.toggleAttribute("showing-callout", false);
}

/**
 * Opens up a new browser window, makes sure its FxA state is initialized, and
 * then runs taskFn (passing it the opened window). When taskFn resolves, the
 * window is closed and an (optional) cleanup function is run.
 *
 * @param {Function|null} doCleanup
 *   An optional async cleanup function to call once the browser window has
 *   closed.
 * @param {Function} taskFn
 *   An async function to run once the browser window has opened and its FxA
 *   state has initialized. This is passed the opened window as its only
 *   argument.
 * @returns {Promise<undefined>}
 */
async function testInNewWindow(doCleanup, taskFn) {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  win.gSync.init();

  await taskFn(win);
  await BrowserTestUtils.closeWindow(win);
  if (doCleanup) {
    await doCleanup();
  }
}

/**
 * Tests that we can change the signed-out icon for the FxA avatar menu via
 * Experimenter. Also ensures that the signed-in icon is not affected by these
 * signed-out variants.
 */

add_setup(async () => {
  UIState.get = () => ({
    status: UIState.STATUS_NOT_CONFIGURED,
  });
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  gSync.init();
});

/**
 * Tests that we use the default icon when not enrolled in the experiment.
 */
add_task(async function test_default() {
  Assert.equal(
    NimbusFeatures.fxaButtonVisibility.getVariable("avatarIconVariant"),
    undefined,
    "Should not start with a NimbusFeature set for the signed-out icon."
  );

  await testInNewWindow(null /* no cleanup */, async win => {
    assertSignedOutIcon(win, DEFAULT_ICON);
    assertSignedInIcon(win);
  });
});

/**
 * Tests that we use the default icon when enrolled in the control branch.
 */
add_task(async function test_control() {
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.fxaButtonVisibility.featureId,
      value: {
        avatarIconVariant: "control",
      },
    },
    { isRollout: true }
  );

  await testInNewWindow(doCleanup, async win => {
    assertSignedOutIcon(win, DEFAULT_ICON);
    assertSignedInIcon(win);
  });
});

/**
 * Tests that we use the human-circle icon when enrolled in the human-circle
 * branch, and that we hide the circle when in the badged state.
 */
add_task(async function test_human_circle() {
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.fxaButtonVisibility.featureId,
      value: {
        avatarIconVariant: "human-circle",
      },
    },
    { isRollout: true }
  );

  await testInNewWindow(doCleanup, async win => {
    assertSignedOutIcon(win, HUMAN_CIRCLE);
    assertBadgedIcon(win, HUMAN_CIRCLE_BADGED);
    assertSignedInIcon(win);
  });
});

/**
 * Tests that we use the fox-circle icon when enrolled in the fox-circle
 * branch, and that we hide the circle when in the badged state.
 */
add_task(async function test_fox_circle() {
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.fxaButtonVisibility.featureId,
      value: {
        avatarIconVariant: "fox-circle",
      },
    },
    { isRollout: true }
  );

  await testInNewWindow(doCleanup, async win => {
    assertSignedOutIcon(win, FOX_CIRCLE);
    assertBadgedIcon(win, FOX_CIRCLE_BADGED);
    assertSignedInIcon(win);
  });
});
