"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const THIRD_PARTY_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.net"
);

const SHIMMABLE_TEST_PAGE = `${TEST_ROOT}shims_test.html`;
const SHIMMABLE_TEST_PAGE_2 = `${TEST_ROOT}shims_test_2.html`;
const SHIMMABLE_TEST_PAGE_3 = `${TEST_ROOT}shims_test_3.html`;
const EMBEDDING_TEST_PAGE = `${THIRD_PARTY_ROOT}iframe_test.html`;
const TEST_PAGE_WITH_SMARTBLOCK_COMPATIBLE_EMBED = `${TEST_ROOT}smartblock_embed_test.html`;

const BLOCKED_TRACKER_URL =
  "//trackertest.org/tests/toolkit/components/url-classifier/tests/mochitest/evil.js";

const DISABLE_SHIM1_PREF = "extensions.webcompat.disabled_shims.MochitestShim";
const DISABLE_SHIM2_PREF = "extensions.webcompat.disabled_shims.MochitestShim2";
const DISABLE_SHIM3_PREF = "extensions.webcompat.disabled_shims.MochitestShim3";
const DISABLE_SHIM4_PREF = "extensions.webcompat.disabled_shims.MochitestShim4";
const GLOBAL_PREF = "extensions.webcompat.enable_shims";
const TRACKING_PREF = "privacy.trackingprotection.enabled";
const SEC_DELAY_PREF = "security.notification_enable_delay";
const SMARTBLOCK_EMBEDS_ENABLED_PREF =
  "extensions.webcompat.smartblockEmbeds.enabled";

const { UrlClassifierTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlClassifierTestUtils.sys.mjs"
);

const WebCompatExtension = new (class WebCompatExtension {
  constructor() {
    const { extension } = WebExtensionPolicy.getByID("webcompat@mozilla.org");
    this.extension = extension;
  }

  async started() {
    await this.extension.promiseBackgroundStarted();
  }

  // Note that the `extensions.background.idle.timeout` pref needs to be set to a
  // a high value if using the following APIs, to avoid intermittent failures due
  // to the background being stopped for being idle.
  async #run(command, ...args) {
    return await SpecialPowers.spawn(
      this.extension.backgroundContext.xulBrowser,
      args,
      command
    );
  }

  async interventionsReady() {
    return this.#run(async function () {
      await content.wrappedJSObject.interventions.ready();
    });
  }

  async shimsReady() {
    return this.#run(async function () {
      await content.wrappedJSObject.shims.ready();
    });
  }

  async updateInterventions(_config) {
    return this.#run(async function (config) {
      return await content.wrappedJSObject.interventions.updateInterventions(
        Cu.cloneInto(config, content)
      );
    }, _config);
  }
})();

async function testShimRuns(
  testPage,
  frame,
  trackersAllowed = true,
  expectOptIn = true
) {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: testPage,
    waitForLoad: true,
  });

  const TrackingProtection =
    tab.ownerGlobal.gProtectionsHandler.blockers.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the tab");
  ok(TrackingProtection.enabled, "TP is enabled");

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [[trackersAllowed, BLOCKED_TRACKER_URL, expectOptIn], frame],
    async (args, _frame) => {
      const window = _frame === undefined ? content : content.frames[_frame];

      await SpecialPowers.spawn(
        window,
        args,
        async (_trackersAllowed, trackerUrl, _expectOptIn) => {
          const shimResult = await content.wrappedJSObject.shimPromise;
          is("shimmed", shimResult, "Shim activated");

          const optInResult = await content.wrappedJSObject.optInPromise;
          is(_expectOptIn, optInResult, "Shim allowed opt in if appropriate");

          const o = content.document.getElementById("shims");
          const cl = o.classList;
          const opts = JSON.parse(o.innerText);
          is(
            undefined,
            opts.branchValue,
            "Shim script did not receive option for other branch"
          );
          is(
            undefined,
            opts.platformValue,
            "Shim script did not receive option for other platform"
          );
          is(
            true,
            opts.simpleOption,
            "Shim script received simple option correctly"
          );
          ok(opts.complexOption, "Shim script received complex option");
          is(
            1,
            opts.complexOption.a,
            "Shim script received complex options correctly #1"
          );
          is(
            "test",
            opts.complexOption.b,
            "Shim script received complex options correctly #2"
          );
          ok(cl.contains("green"), "Shim affected page correctly");
        }
      );
    }
  );

  await BrowserTestUtils.removeTab(tab);
}

async function testShimDoesNotRun(
  trackersAllowed = false,
  testPage = SHIMMABLE_TEST_PAGE
) {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: testPage,
    waitForLoad: true,
  });

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [trackersAllowed, BLOCKED_TRACKER_URL],
    async (_trackersAllowed, trackerUrl) => {
      const shimResult = await content.wrappedJSObject.shimPromise;
      is("did not shim", shimResult, "Shim did not activate");

      ok(
        !content.document.getElementById("shims").classList.contains("green"),
        "Shim script did not run"
      );

      is(
        _trackersAllowed ? "ALLOWED" : "BLOCKED",
        await new Promise(resolve => {
          const s = content.document.createElement("script");
          s.src = trackerUrl;
          s.onload = () => resolve("ALLOWED");
          s.onerror = () => resolve("BLOCKED");
          content.document.head.appendChild(s);
        }),
        "Normally-blocked resources blocked if appropriate"
      );
    }
  );

  await BrowserTestUtils.removeTab(tab);
}

async function closeProtectionsPanel(win = window) {
  let protectionsPopup = win.document.getElementById("protections-popup");
  if (!protectionsPopup) {
    return;
  }
  let popuphiddenPromise = BrowserTestUtils.waitForEvent(
    protectionsPopup,
    "popuphidden"
  );

  PanelMultiView.hidePopup(protectionsPopup);
  await popuphiddenPromise;
}

async function openProtectionsPanel(win = window) {
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    win,
    "popupshown",
    true,
    e => e.target.id == "protections-popup"
  );

  win.gProtectionsHandler.showProtectionsPopup();

  await popupShownPromise;
}

async function loadSmartblockPageOnTab(tab) {
  let smartblockScriptFinished = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "smartblockEmbedScriptFinished",
    false,
    null,
    true
  );

  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    TEST_PAGE_WITH_SMARTBLOCK_COMPATIBLE_EMBED
  );

  return smartblockScriptFinished;
}

async function clickOnPagePlaceholder(tab) {
  // Setup promise for listening for protections panel open
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    window,
    "popupshown",
    true,
    e => e.target.id == "protections-popup"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // Check that the "embed" was replaced with a placeholder
    let placeholder = content.document.querySelector(
      ".shimmed-embedded-content"
    );
    ok(placeholder, "Embed is replaced with a placeholder");

    // Get the button element from the placeholder
    let shadowRoot = placeholder.openOrClosedShadowRoot;
    ok(shadowRoot, "Shadow root exists");

    // Check that all elements are present
    let placeholderButton = shadowRoot.querySelector(
      "#smartblock-placeholder-button"
    );
    ok(placeholderButton, "Placeholder button exists");

    let placeholderTitle = shadowRoot.querySelector(
      "#smartblock-placeholder-title"
    );
    ok(placeholderTitle, "Placeholder title exists");

    let placeholderLabel = shadowRoot.querySelector(
      "#smartblock-placeholder-desc"
    );
    ok(placeholderLabel, "Placeholder description exists");

    let placeholderImage = shadowRoot.querySelector(
      "#smartblock-placeholder-image"
    );
    ok(placeholderImage, "Placeholder image exists");

    // Click button to open protections panel
    await EventUtils.synthesizeMouseAtCenter(placeholderButton, {}, content);
  });

  // If this await finished, then protections panel is open
  return popupShownPromise;
}
