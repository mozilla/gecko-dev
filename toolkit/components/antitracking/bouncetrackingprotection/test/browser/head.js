/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);

const SITE_A = "example.com";
const ORIGIN_A = `https://${SITE_A}`;

const SITE_B = "example.org";
const ORIGIN_B = `https://${SITE_B}`;

const SITE_C = "example.net";
const ORIGIN_C = `https://${SITE_C}`;

const SITE_TRACKER = "itisatracker.org";
const ORIGIN_TRACKER = `https://${SITE_TRACKER}`;

const SITE_TRACKER_B = "trackertest.org";
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const ORIGIN_TRACKER_B = `http://${SITE_TRACKER_B}`;

// Test message used for observing when the record-bounces method in
// BounceTrackingProtection.cpp has finished.
const OBSERVER_MSG_RECORD_BOUNCES_FINISHED = "test-record-bounces-finished";

const ROOT_DIR = getRootDirectory(gTestPath);

/**
 * Get the base url for the current test directory using the given origin.
 * @param {string} origin - Origin to use in URL.
 * @returns {string} - Generated URL as a string.
 */
function getBaseUrl(origin) {
  return ROOT_DIR.replace("chrome://mochitests/content", origin);
}

/**
 * Constructs a url for an intermediate "bounce" hop which represents a tracker.
 * @param {*} options - URL generation options.
 * @param {('server'|'client')} options.bounceType - Redirect type to use for
 * the bounce.
 * @param {string} [options.bounceOrigin] - The origin of the bounce URL.
 * @param {string} [options.targetURL] - URL to redirect to after the bounce.
 * @param {('cookie-server'|'cookie-client'|'localStorage'|'indexedDB')}
 * [options.setState] Type of state to set during the redirect. Defaults to non
 * stateful redirect.
 * @param {boolean} [options.setStateSameSiteFrame=false] - Whether to set the
 * state in a sub frame that is same site to the top window.
 * @param {boolean} [options.setStateCrossSiteFrame=false] - Whether to set the
 * state in a sub frame that is cross-site to the top window.
 * @param {boolean} [options.setStateInWebWorker=false] - Whether to set the
 * state in a web worker. This only supports setState == "indexedDB".
 * @param {boolean} [options.setStateInWebWorker=false] - Whether to set the
 * state in a nested web worker. Otherwise the same as setStateInWebWorker.
 * @param {('same-site'|'cross-site')} [options.setCookieViaImage] - Whether to
 * set the state via an image request. Only applies to setState ==
 * "cookie-server".
 * @param {number} [options.statusCode] - HTTP status code to use for server
 * side redirect. Only applies to bounceType == "server".
 * @param {number} [options.redirectDelayMS] - How long to wait before
 * redirecting. Only applies to bounceType == "client".
 * @returns {URL} Generated URL which points to an endpoint performing the
 * redirect.
 */
function getBounceURL({
  bounceType,
  bounceOrigin = ORIGIN_TRACKER,
  targetURL = new URL(getBaseUrl(ORIGIN_B) + "file_start.html"),
  setState = null,
  setStateSameSiteFrame = false,
  setStateCrossSiteFrame = false,
  setStateInWebWorker = false,
  setStateInNestedWebWorker = false,
  setCookieViaImage = null,
  statusCode = 302,
  redirectDelayMS = 50,
}) {
  if (!["server", "client"].includes(bounceType)) {
    throw new Error("Invalid bounceType");
  }

  let bounceFile =
    bounceType == "client" ? "file_bounce.html" : "file_bounce.sjs";

  let bounceUrl = new URL(getBaseUrl(bounceOrigin) + bounceFile);

  let { searchParams } = bounceUrl;
  searchParams.set("target", targetURL.href);
  if (setState) {
    searchParams.set("setState", setState);
  }

  if (setStateInWebWorker) {
    if (setState != "indexedDB") {
      throw new Error(
        "setStateInWebWorker only supports setState == 'indexedDB'"
      );
    }
    searchParams.set("setStateInWebWorker", setStateInWebWorker);
  }
  if (setStateInNestedWebWorker) {
    if (setState != "indexedDB") {
      throw new Error(
        "setStateInNestedWebWorker only supports setState == 'indexedDB'"
      );
    }
    searchParams.set("setStateInNestedWebWorker", setStateInNestedWebWorker);
  }

  if (bounceType == "server") {
    searchParams.set("statusCode", statusCode);
  } else if (bounceType == "client") {
    searchParams.set("redirectDelay", redirectDelayMS);
  }

  // For bounces in iframes the helper needs the URI of the iframe which sets
  // the state. Since this reuses bounceUrl it needs to be last in this
  // function.
  if (setStateSameSiteFrame || setStateCrossSiteFrame) {
    // Construct the URI for the iframe.
    let bounceUrlIframe = new URL(bounceUrl.href);

    // Let the frame know it's a third party iframe.
    bounceUrlIframe.searchParams.set("isThirdParty", true);

    // If a server side cookie needs to be set we have to use the server script
    // in the iframe.
    if (setState == "cookie-server") {
      bounceUrlIframe.pathname = bounceUrlIframe.pathname.replace(
        "file_bounce.html",
        "file_bounce.sjs"
      );
    }
    if (setStateSameSiteFrame) {
      searchParams.set("setStateInFrameWithURI", bounceUrlIframe.href);
    } else {
      bounceUrlIframe.host = SITE_C;
      searchParams.set("setStateInFrameWithURI", bounceUrlIframe.href);
    }
  } else if (setCookieViaImage) {
    let imageOrigin =
      setCookieViaImage == "same-site" ? bounceOrigin : ORIGIN_C;
    let imageURL = new URL(getBaseUrl(imageOrigin) + "file_image.png");

    if (setState != "cookie-server") {
      throw new Error(
        "setCookieViaImage only supports setState == 'cookie-server'"
      );
    }
    searchParams.set("setCookieViaImageWithURI", imageURL.href);
  }

  return bounceUrl;
}

/**
 * Insert an <a href/> element with the given target and perform a synthesized
 * click on it.
 * @param {MozBrowser} browser - Browser to insert the link in.
 * @param {URL} targetURL - Destination for navigation.
 * @param {Object} options - Additional options.
 * @param {string} [options.spawnWindow] - If set to "newTab" or "popup" the
 * link will be opened in a new tab or popup window respectively. If unset the
 * link is opened in the given browser.
 * @returns {Promise} Resolves once the click is done. Does not wait for
 * navigation or load.
 */
async function navigateLinkClick(
  browser,
  targetURL,
  { spawnWindow = null } = {}
) {
  if (spawnWindow && !["newTab", "popup"].includes(spawnWindow)) {
    throw new Error(`Invalid option '${spawnWindow}' for spawnWindow`);
  }

  await SpecialPowers.spawn(
    browser,
    [targetURL.href, spawnWindow],
    async (targetURL, spawnWindow) => {
      let link = content.document.createElement("a");
      link.id = "link";
      link.textContent = "Click Me";
      link.style.display = "block";
      link.style.fontSize = "40px";

      // For opening a popup we attach an event listener to trigger via click.
      if (spawnWindow) {
        link.href = "#";
        link.addEventListener("click", event => {
          event.preventDefault();
          if (spawnWindow == "newTab") {
            // Open a new tab.
            content.window.open(targetURL, "bounce");
          } else {
            // Open a popup window.
            content.window.open(targetURL, "bounce", "height=200,width=200");
          }
        });
      } else {
        // For regular navigation add href and click.
        link.href = targetURL;
      }

      content.document.body.appendChild(link);

      // TODO: Bug 1892091: Use EventUtils.synthesizeMouse instead for a real click.
      SpecialPowers.wrap(content.document).notifyUserGestureActivation();
      content.document.userInteractionForTesting();
      link.click();
    }
  );
}

/**
 * Wait for the record-bounces method to run for the given tab / browser.
 * @param {browser} browser - Browser element which represents the tab we want
 * to observe.
 * @returns {Promise} Promise which resolves once the record-bounces method has
 * run for the given browser.
 */
async function waitForRecordBounces(browser) {
  let { browserId } = browser.browsingContext;
  info(
    `waitForRecordBounces: Waiting for record bounces for browser: ${browserId}.`
  );

  await TestUtils.topicObserved(
    OBSERVER_MSG_RECORD_BOUNCES_FINISHED,
    subject => {
      // Ensure the message was dispatched for the browser we're interested in.
      let propBag = subject.QueryInterface(Ci.nsIPropertyBag2);
      return browserId == propBag.getProperty("browserId");
    }
  );

  info(`waitForRecordBounces: Recorded bounces for browser ${browserId}.`);
}

/**
 * Test helper which loads an initial blank page, then navigates to a url which
 * performs a bounce. Checks that the bounce hosts are properly identified as
 * trackers.
 * @param {object} options - Test Options.
 * @param {('server'|'client')} options.bounceType - Whether to perform a client
 * or server side redirect.
 * @param {('cookie-server'|'cookie-client'|'localStorage'|'indexedDB')}
 * [options.setState] Type of state to set during the redirect. Defaults to non
 * stateful redirect.
 * @param {boolean} [options.setStateSameSiteFrame=false] - Whether to set the
 * state in a sub frame that is same site to the top window.
 * @param {boolean} [options.setStateCrossSiteFrame=false] - Whether to set the
 * state in a sub frame that is cross-site to the top window.
 * @param {boolean} [options.setStateInWebWorker=false] - Whether to set the
 * state in a web worker. This only supports setState == "indexedDB".
 * @param {boolean} [options.setStateInWebWorker=false] - Whether to set the
 * state in a nested web worker. Otherwise the same as setStateInWebWorker.
 * @param {('same-site'|'cross-site')} [options.setCookieViaImage] - Whether to
 * set the state via an image request. Only applies to setState ==
 * "cookie-server".
 * @param {boolean} [options.expectRecordBounces=true] - Whether the record
 * bounces algorithm runs and we should wait for the test message. This
 * shouldn't run when the feature is disabled.
 * @param {boolean} [options.expectCandidate=true] - Expect the redirecting site
 * to be identified as a bounce tracker (candidate).
 * @param {boolean} [options.expectPurge=true] - Expect the redirecting site to
 * have its storage purged.
 * @param {OriginAttributes} [options.originAttributes={}] - Origin attributes
 * to use for the test. This determines whether the test is run in normal
 * browsing, a private window or a container tab. By default the test is run in
 * normal browsing.
 * @param {function} [options.postBounceCallback] - Optional function to run
 * after the bounce has completed.
 * @param {boolean} [options.skipSiteDataCleanup=false] - Skip the cleanup of
 * site data after the test. When this is enabled the caller is responsible for
 * cleaning up site data.
 * @param {boolean} [options.skipBounceTrackingProtectionCleanup=false] - Skip
 * the cleanup of BounceTrackingProtection state. When this is enabled the
 * caller is responsible for cleaning BTP state.
 * @param {boolean} [options.closeTabAfterBounce=false] - Close the tab right
 * after the bounce completes before the extended navigation ends as the result
 * of a timeout or user interaction.
 */
async function runTestBounce(options = {}) {
  let {
    bounceType,
    setState = null,
    setStateSameSiteFrame = false,
    setStateCrossSiteFrame = false,
    setStateInWebWorker = false,
    setStateInNestedWebWorker = false,
    setCookieViaImage = null,
    expectRecordBounces = true,
    expectCandidate = true,
    expectPurge = true,
    originAttributes = {},
    postBounceCallback = () => {},
    skipSiteDataCleanup = false,
    skipBounceTrackingProtectionCleanup = false,
    closeTabAfterBounce = false,
  } = options;
  info(`runTestBounce ${JSON.stringify(options)}`);

  let btpIsDisabled =
    Services.prefs.getIntPref("privacy.bounceTrackingProtection.mode") ==
    Ci.nsIBounceTrackingProtection.MODE_DISABLED;

  let bounceTrackingProtection;
  try {
    bounceTrackingProtection = Cc[
      "@mozilla.org/bounce-tracking-protection;1"
    ].getService(Ci.nsIBounceTrackingProtection);
  } catch (error) {
    // Only in MODE_DISABLED this may throw because
    // `BounceTrackingProtection::GetSingleton` will return `nullptr`.
    if (!btpIsDisabled) {
      throw error;
    }
  }

  if (btpIsDisabled) {
    Assert.ok(!expectCandidate, "Expect no classification in disabled mode.");
    Assert.ok(
      !expectRecordBounces,
      "Expect no record bounces in disabled mode."
    );
    Assert.ok(!expectPurge, "Expect no purge in disabled mode.");
  } else {
    Assert.ok(
      bounceTrackingProtection,
      "BTP singleton must be available in any of the 'enabled' modes."
    );
  }

  if (bounceTrackingProtection) {
    Assert.equal(
      bounceTrackingProtection.testGetBounceTrackerCandidateHosts(
        originAttributes
      ).length,
      0,
      "No bounce tracker hosts initially."
    );
    Assert.equal(
      bounceTrackingProtection.testGetUserActivationHosts(originAttributes)
        .length,
      0,
      "No user activation hosts initially."
    );
  }

  let win = window;
  let { privateBrowsingId, userContextId } = originAttributes;
  let usePrivateWindow =
    privateBrowsingId != null &&
    privateBrowsingId !=
      Services.scriptSecurityManager.DEFAULT_PRIVATE_BROWSING_ID;
  if (userContextId != null && userContextId > 0 && usePrivateWindow) {
    throw new Error("userContextId is not supported in private windows");
  }

  if (usePrivateWindow) {
    win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  }

  let initialURL = getBaseUrl(ORIGIN_A) + "file_start.html";
  let tab = win.gBrowser.addTab(initialURL, {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    userContextId,
  });
  win.gBrowser.selectedTab = tab;

  let browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser, true, initialURL);

  let promiseRecordBounces;
  if (expectRecordBounces) {
    promiseRecordBounces = waitForRecordBounces(browser);
  }

  // The final destination after the bounce.
  let targetURL = new URL(getBaseUrl(ORIGIN_B) + "file_start.html");

  // Wait for the final site to be loaded which complete the BounceTrackingRecord.
  let targetURLLoadedPromise = BrowserTestUtils.browserLoaded(
    browser,
    false,
    targetURL
  );

  // Navigate through the bounce chain.
  await navigateLinkClick(
    browser,
    getBounceURL({
      bounceType,
      targetURL,
      setState,
      setStateSameSiteFrame,
      setStateCrossSiteFrame,
      setStateInWebWorker,
      setStateInNestedWebWorker,
      setCookieViaImage,
    })
  );

  await targetURLLoadedPromise;

  // Caller requested to close the tab early. This should happen before the
  // extended navigation ends due to timeout or user interaction with the
  // destination site.
  // In this case the extended navigation end is triggered by the tab close
  // itself.
  if (closeTabAfterBounce) {
    // This either closes the normal or private browsing tab depending on
    // 'usePrivateWindow'.
    BrowserTestUtils.removeTab(tab);

    // Make sure these don't get reused, the tab has been closed.
    tab = null;
    browser = null;
  } else {
    // Tab is still open.
    // Navigate again with user gesture which triggers
    // BounceTrackingProtection::RecordStatefulBounces. We could rely on the
    // timeout (mClientBounceDetectionTimeout) here but that can cause races in
    // debug where the load is quite slow.
    let finalTargetURL = new URL(getBaseUrl(ORIGIN_C) + "file_start.html");
    let finalLoadPromise = BrowserTestUtils.browserLoaded(
      browser,
      true,
      initialURL.href
    );
    await navigateLinkClick(browser, finalTargetURL);
    await finalLoadPromise;
  }

  if (expectRecordBounces) {
    info("Waiting for record-bounces to complete.");
    await promiseRecordBounces;
  } else {
    // If we don't expect classification to happen only wait for navigation from
    // the navigateLinkClick to complete. This navigation would trigger
    // RecordStatefulBounces if the protection was enabled. Give
    // RecordStatefulBounces time to run after navigation.
    await new Promise(resolve => setTimeout(resolve, 0));
  }

  if (btpIsDisabled) {
    // In MODE_DISABLED `bounceTrackingProtection` may still be defined if it
    // was previously accessed in an enabled state. In that case make sure
    // nothing is recorded.
    if (bounceTrackingProtection) {
      Assert.deepEqual(
        bounceTrackingProtection
          .testGetBounceTrackerCandidateHosts(originAttributes)
          .map(entry => entry.siteHost),
        [],
        "Should not have identified any bounce trackers"
      );
      Assert.deepEqual(
        bounceTrackingProtection
          .testGetUserActivationHosts(originAttributes)
          .map(entry => entry.siteHost),
        [],
        "Should not have recorded any user activation"
      );
    } else {
      info("BTP singleton is unavailable because mode is MODE_DISABLED.");
    }
  } else {
    // Any of the "enabled" modes.
    Assert.deepEqual(
      bounceTrackingProtection
        .testGetBounceTrackerCandidateHosts(originAttributes)
        .map(entry => entry.siteHost),
      expectCandidate ? [SITE_TRACKER] : [],
      `Should ${
        expectCandidate ? "" : "not "
      }have identified ${SITE_TRACKER} as a bounce tracker.`
    );

    let expectedUserActivationHosts = [SITE_A];
    if (!closeTabAfterBounce) {
      // If we didn't close the tab early we should have user activation for the
      // destination site.
      expectedUserActivationHosts.push(SITE_B);
    }

    Assert.deepEqual(
      bounceTrackingProtection
        .testGetUserActivationHosts(originAttributes)
        .map(entry => entry.siteHost)
        .sort(),
      expectedUserActivationHosts.sort(),
      "Should only have user activation for sites where we clicked links."
    );
  }

  // If the caller specified a function to run after the bounce, run it now.
  await postBounceCallback();

  if (bounceTrackingProtection) {
    // Run tracker purging. If the feature is disabled this throws.
    let mode = Services.prefs.getIntPref(
      "privacy.bounceTrackingProtection.mode"
    );
    let expectPurgingToThrow =
      mode != Ci.nsIBounceTrackingProtection.MODE_ENABLED &&
      mode != Ci.nsIBounceTrackingProtection.MODE_ENABLED_DRY_RUN;

    if (expectPurgingToThrow) {
      await Assert.rejects(
        bounceTrackingProtection.testRunPurgeBounceTrackers(),
        /NS_ERROR_NOT_AVAILABLE/,
        "testRunPurgeBounceTrackers should reject when BTP is disabled."
      );
    } else {
      Assert.deepEqual(
        await bounceTrackingProtection.testRunPurgeBounceTrackers(),
        expectPurge ? [SITE_TRACKER] : [],
        `Should ${expectPurge ? "" : "not "}purge state for ${SITE_TRACKER}.`
      );

      info("Testing the purge log.");
      let purgeLog =
        bounceTrackingProtection.testGetRecentlyPurgedTrackers(
          originAttributes
        );
      // Purges are only logged in (fully) enabled mode. Dry-run mode does not
      // log purges.
      if (expectPurge && mode == Ci.nsIBounceTrackingProtection.MODE_ENABLED) {
        Assert.equal(
          purgeLog.length,
          1,
          "Should have one tracker in purge log."
        );
        let { siteHost, timeStamp, purgeTime } = purgeLog[0];

        Assert.equal(
          siteHost,
          SITE_TRACKER,
          `The purge log entry should be for site host '${SITE_TRACKER}'`
        );
        Assert.greater(
          timeStamp,
          0,
          "The purge log entry should have a valid timestamp for bounce time."
        );
        Assert.greater(
          purgeTime,
          0,
          "The purge log entry should have a valid timestamp for purge time."
        );
        Assert.greaterOrEqual(
          purgeTime,
          timeStamp,
          "The purge time should be greater or equal to bounce time."
        );
      } else {
        Assert.equal(
          purgeLog.length,
          0,
          "Should have no trackers in purge log."
        );
      }
    }
  } else {
    info("BTP is disabled. Skipping purge call.");
  }

  // Clean up
  // Tab might have been closed early.
  if (tab) {
    BrowserTestUtils.removeTab(tab);
  }
  if (usePrivateWindow) {
    await BrowserTestUtils.closeWindow(win);

    if (bounceTrackingProtection) {
      info(
        "Closing the last PBM window should trigger a purge of all PBM state."
      );
      Assert.ok(
        !bounceTrackingProtection.testGetBounceTrackerCandidateHosts(
          originAttributes
        ).length,
        "No bounce tracker hosts after closing private window."
      );
      Assert.ok(
        !bounceTrackingProtection.testGetUserActivationHosts(originAttributes)
          .length,
        "No user activation hosts after closing private window."
      );
    }
  }
  if (!skipBounceTrackingProtectionCleanup) {
    bounceTrackingProtection?.clearAll();
  }

  if (!skipSiteDataCleanup) {
    await SiteDataTestUtils.clear();
  }
}
