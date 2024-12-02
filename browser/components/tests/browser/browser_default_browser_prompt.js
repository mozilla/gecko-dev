/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DefaultBrowserCheck } = ChromeUtils.importESModule(
  "resource:///modules/BrowserGlue.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);

const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
);
const CHECK_PREF = "browser.shell.checkDefaultBrowser";

function showAndWaitForModal(callback) {
  const promise = BrowserTestUtils.promiseAlertDialog(null, undefined, {
    callback,
    isSubDialog: true,
  });
  DefaultBrowserCheck.prompt(BrowserWindowTracker.getTopWindow());
  return promise;
}

const TELEMETRY_NAMES = ["accept check", "accept", "cancel check", "cancel"];

let testSetDefaultSpotlight = {
  id: "TEST_MESSAGE",
  template: "spotlight",
  content: {
    template: "multistage",
    id: "SET_DEFAULT_SPOTLIGHT",
    screens: [
      {
        id: "PROMPT_CLONE",
        content: {
          isSystemPromptStyleSpotlight: true,
          title: {
            fontSize: "13px",
            raw: "Make Nightly your default browser?",
          },
          subtitle: {
            fontSize: "13px",
            raw: "Keep Nightly at your fingertips — make it your default browser and keep it in your Dock.",
          },
        },
      },
    ],
  },
};
function AssertHistogram(histogram, name, expect = 1) {
  TelemetryTestUtils.assertHistogram(
    histogram,
    TELEMETRY_NAMES.indexOf(name),
    expect
  );
}
function getHistogram() {
  return TelemetryTestUtils.getAndClearHistogram("BROWSER_SET_DEFAULT_RESULT");
}

add_task(async function proton_shows_prompt() {
  mockShell();
  ShellService._checkedThisSession = false;

  await SpecialPowers.pushPrefEnv({
    set: [
      [CHECK_PREF, true],
      ["browser.shell.didSkipDefaultBrowserCheckOnFirstRun", true],
    ],
  });

  const willPrompt = await DefaultBrowserCheck.willCheckDefaultBrowser();

  Assert.equal(
    willPrompt,
    !AppConstants.DEBUG,
    "Show default browser prompt with proton on non-debug builds"
  );
});

add_task(async function not_now() {
  const histogram = getHistogram();
  await showAndWaitForModal(win => {
    win.document.querySelector("dialog").getButton("cancel").click();
  });

  Assert.equal(
    Services.prefs.getBoolPref(CHECK_PREF),
    true,
    "Canceling keeps pref true"
  );
  AssertHistogram(histogram, "cancel");
});

add_task(async function stop_asking() {
  const histogram = getHistogram();

  await showAndWaitForModal(win => {
    const dialog = win.document.querySelector("dialog");
    dialog.querySelector("checkbox").click();
    dialog.getButton("cancel").click();
  });

  Assert.equal(
    Services.prefs.getBoolPref(CHECK_PREF),
    false,
    "Canceling with checkbox checked clears the pref"
  );
  AssertHistogram(histogram, "cancel check");
});

add_task(async function primary_default() {
  const mock = mockShell({ isPinned: true, isPinnedToStartMenu: true });
  const histogram = getHistogram();

  await showAndWaitForModal(win => {
    win.document.querySelector("dialog").getButton("accept").click();
  });

  Assert.equal(
    mock.setAsDefault.callCount,
    1,
    "Primary button sets as default"
  );
  Assert.equal(
    mock.pinCurrentAppToTaskbarAsync.callCount,
    0,
    "Primary button doesn't pin if already pinned"
  );
  Assert.equal(
    mock.pinCurrentAppToStartMenuAsync.callCount,
    0,
    "Primary button doesn't pin if already pinned"
  );
  AssertHistogram(histogram, "accept");
});

add_task(async function primary_pin() {
  const mock = mockShell({ canPin: true });
  const histogram = getHistogram();

  await showAndWaitForModal(win => {
    win.document.querySelector("dialog").getButton("accept").click();
  });

  Assert.equal(
    mock.setAsDefault.callCount,
    1,
    "Primary button sets as default"
  );
  if (AppConstants.platform == "win") {
    Assert.equal(
      mock.pinCurrentAppToTaskbarAsync.callCount,
      1,
      "Primary button also pins"
    );
    if (Services.sysinfo.getProperty("hasWinPackageId")) {
      Assert.equal(
        mock.pinCurrentAppToStartMenuAsync.callCount,
        1,
        "Primary button also pins to Windows start menu on MSIX"
      );
    }
  }
  AssertHistogram(histogram, "accept");
});

add_task(async function showDefaultPrompt() {
  let sb = sinon.createSandbox();
  const win2 = await BrowserTestUtils.openNewBrowserWindow();

  const willPromptStub = sb
    .stub(DefaultBrowserCheck, "willCheckDefaultBrowser")
    .returns(true);
  const promptSpy = sb.spy(DefaultBrowserCheck, "prompt");
  await ExperimentAPI.ready();
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.setToDefaultPrompt.featureId,
      value: {
        showSpotlightPrompt: false,
        message: {},
      },
    },
    {
      slug: "test-prompt-style-spotlight",
    },
    {
      isRollout: true,
    }
  );

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  Assert.equal(willPromptStub.callCount, 1, "willCheckDefaultBrowser called");
  Assert.equal(promptSpy.callCount, 1, "default prompt should be called");

  await sb.restore();

  doExperimentCleanup();
  await BrowserTestUtils.closeWindow(win2);
});

add_task(async function promptStoresImpressionAndDisableTimestamps() {
  await showAndWaitForModal(win => {
    const dialog = win.document.querySelector("dialog");
    dialog.querySelector("checkbox").click();
    dialog.getButton("cancel").click();
  });

  const impressionTimestamp = Services.prefs.getCharPref(
    "browser.shell.mostRecentDefaultPromptSeen"
  );
  const disabledTimestamp = Services.prefs.getCharPref(
    "browser.shell.userDisabledDefaultCheck"
  );

  const now = Math.floor(Date.now() / 1000);
  const oneHourInMs = 60 * 60 * 1000;

  Assert.ok(
    impressionTimestamp &&
      now - parseInt(impressionTimestamp, 10) <= oneHourInMs,
    "Prompt impression timestamp is stored"
  );

  Assert.ok(
    disabledTimestamp && now - parseInt(disabledTimestamp, 10) <= oneHourInMs,
    "Selecting checkbox stores timestamp of when user disabled the prompt"
  );
});

add_task(async function showPromptStyleSpotlight() {
  let sandbox = sinon.createSandbox();

  const win = await BrowserTestUtils.openNewBrowserWindow();

  const willPromptStub = sandbox
    .stub(DefaultBrowserCheck, "willCheckDefaultBrowser")
    .returns(true);
  const showSpotlightSpy = sandbox.spy(SpecialMessageActions, "handleAction");

  await ExperimentAPI.ready();
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.setToDefaultPrompt.featureId,
      value: {
        showSpotlightPrompt: true,
        message: testSetDefaultSpotlight,
      },
    },
    {
      slug: "test-prompt-style-spotlight-2",
    },
    {
      isRollout: true,
    }
  );

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  Assert.equal(willPromptStub.callCount, 1, "willCheckDefaultBrowser called");
  Assert.equal(showSpotlightSpy.callCount, 1, "handleAction should  be called");

  ok(
    showSpotlightSpy.calledWith({
      type: "SHOW_SPOTLIGHT",
      data: testSetDefaultSpotlight,
    }),
    "handleAction called with right args"
  );

  doExperimentCleanup();
  await sandbox.restore();
  await BrowserTestUtils.closeWindow(win);
});
