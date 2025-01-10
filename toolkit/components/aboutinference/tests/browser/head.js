Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/head.js",
  this
);

async function setupRemoteClient() {
  const { removeMocks, remoteClients } = await createAndMockMLRemoteSettings({
    autoDownloadFromRemoteSettings: false,
  });

  return {
    remoteClients,
    async cleanup() {
      await removeMocks();
      await waitForCondition(
        () => EngineProcess.areAllEnginesTerminated(),
        "Waiting for all of the engines to be terminated.",
        100,
        200
      );
    },
  };
}

async function runInferenceProcess(remoteClients) {
  info("Building the egnine process");

  const { createEngine } = ChromeUtils.importESModule(
    "chrome://global/content/ml/EngineProcess.sys.mjs"
  );

  const engine = await createEngine({ taskName: "moz-echo" });
  const inferencePromise = engine.run({ data: "This gets echoed." });
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);
  Assert.equal(
    (await inferencePromise).output.echo,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );
}

/**
 * The mochitest runs in the parent process. This function opens up a new tab,
 * navigates to about:inference, and passes the test requirements into the content process.
 *
 * @param {object} options - The options object.
 * @param {boolean} options.disabled - Flag to disable the inference functionality.
 * @param {Function} options.runInPage - The function to run in the content process.
 * @param {Array} [options.prefs] - An array of additional preferences to set.
 * @param {boolean} options.runInference - If true, runs an inference task
 *
 * @returns {Promise<void>} A promise that resolves when the test is complete.
 */
async function openAboutInference({
  disabled,
  runInPage,
  prefs,
  runInference = false,
}) {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.ml.enable", !disabled],
      ["browser.ml.logLevel", "Debug"],
      ["dom.webgpu.enabled", !disabled],
      ["dom.webgpu.workers.enabled", !disabled],
      ...(prefs ?? []),
    ],
  });

  let cleanup;
  let remoteClients;
  // run inference
  if (runInference) {
    let set = await setupRemoteClient();
    cleanup = set.cleanup;
    remoteClients = set.remoteClients;

    await runInferenceProcess(remoteClients);
  }
  /**
   * Collect any relevant selectors for the page here.
   */
  const selectors = {
    pageHeader: '[data-l10n-id="about-inference-header"]',
    warning: "moz-message-bar#warning",
    processes: "div#procInfoTableContainer",
  };

  // Start the tab at a blank page.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    BLANK_PAGE,
    true // waitForLoad
  );

  // Now load the about:inference page, since the actor could be mocked.
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, "about:inference");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await ContentTask.spawn(tab.linkedBrowser, { selectors }, runInPage);

  if (runInference) {
    await EngineProcess.destroyMLEngine();
    await cleanup();
  }
  await loadBlankPage();
  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
}

/**
 * Loads the blank-page URL.
 *
 * This is useful for resetting the state during cleanup, and also
 * before starting a test, to further help ensure that there is no
 * unintentional state left over from test case.
 */
async function loadBlankPage() {
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, BLANK_PAGE);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
}
