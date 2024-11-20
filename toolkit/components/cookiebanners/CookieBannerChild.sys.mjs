/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  setInterval: "resource://gre/modules/Timer.sys.mjs",
  clearInterval: "resource://gre/modules/Timer.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serviceMode",
  "cookiebanners.service.mode",
  Ci.nsICookieBannerService.MODE_DISABLED
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serviceModePBM",
  "cookiebanners.service.mode.privateBrowsing",
  Ci.nsICookieBannerService.MODE_DISABLED
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "prefDetectOnly",
  "cookiebanners.service.detectOnly",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "bannerClickingEnabled",
  "cookiebanners.bannerClicking.enabled",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "cleanupTimeoutAfterLoad",
  "cookiebanners.bannerClicking.timeoutAfterLoad"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "cleanupTimeoutAfterDOMContentLoaded",
  "cookiebanners.bannerClicking.timeoutAfterDOMContentLoaded"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "pollingInterval",
  "cookiebanners.bannerClicking.pollingInterval",
  500
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "testing",
  "cookiebanners.bannerClicking.testing",
  false
);

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "CookieBannerChild",
    maxLogLevelPref: "cookiebanners.bannerClicking.logLevel",
  });
});

export class CookieBannerChild extends JSWindowActorChild {
  // Caches the enabled state to ensure we only compute it once for the lifetime
  // of the actor. Particularly the private browsing check can be expensive.
  #isEnabledCached = null;
  #clickRules;
  #abortController = new AbortController();
  #timeoutSignalController = new AbortController();
  #timeoutTimerID;
  #hasActiveObserver = false;
  // Indicates whether the page "load" event occurred.
  #didLoad = false;

  // Indicates whether we should stop running the cookie banner handling
  // mechanism because it has been previously executed for the site. So, we can
  // cool down the cookie banner handing to improve performance.
  #isCooledDownInSession = false;

  handleEvent(event) {
    if (!this.#isEnabled) {
      // Automated tests may still expect the test message to be sent.
      this.#maybeSendTestMessage();
      return;
    }

    switch (event.type) {
      case "DOMContentLoaded":
        this.#onDOMContentLoaded();
        break;
      case "load":
        this.#onLoad();
        break;
      default:
        lazy.logConsole.warn(`Unexpected event ${event.type}.`, event);
    }
  }

  get #isPrivateBrowsing() {
    return lazy.PrivateBrowsingUtils.isContentWindowPrivate(this.contentWindow);
  }

  /**
   * Whether the feature is enabled based on pref state.
   * @type {boolean} true if feature is enabled, false otherwise.
   */
  get #isEnabled() {
    if (this.#isEnabledCached != null) {
      return this.#isEnabledCached;
    }

    let checkIsEnabled = () => {
      if (!lazy.bannerClickingEnabled) {
        return false;
      }
      if (this.#isPrivateBrowsing) {
        return lazy.serviceModePBM != Ci.nsICookieBannerService.MODE_DISABLED;
      }
      return lazy.serviceMode != Ci.nsICookieBannerService.MODE_DISABLED;
    };

    this.#isEnabledCached = checkIsEnabled();
    return this.#isEnabledCached;
  }

  /**
   * Whether the feature is enabled in detect-only-mode where cookie banner
   * detection events are dispatched, but banners aren't handled.
   * @type {boolean} true if feature mode is enabled, false otherwise.
   */
  get #isDetectOnly() {
    // We can't be in detect-only-mode if fully disabled.
    if (!this.#isEnabled) {
      return false;
    }
    return lazy.prefDetectOnly;
  }

  /**
   * @returns {boolean} Whether we handled a banner for the current load by
   * injecting cookies.
   */
  get #hasInjectedCookieForCookieBannerHandling() {
    return this.docShell?.currentDocumentChannel?.loadInfo
      ?.hasInjectedCookieForCookieBannerHandling;
  }

  /**
   * Checks whether we handled a banner for this site by injecting cookies and
   * dispatches events.
   * @returns {boolean} Whether we handled the banner and dispatched events.
   */
  #dispatchEventsForBannerHandledByInjection() {
    if (
      !this.#hasInjectedCookieForCookieBannerHandling ||
      this.#isCooledDownInSession
    ) {
      return false;
    }
    // Strictly speaking we don't actively detect a banner when we handle it by
    // cookie injection. We still dispatch "cookiebannerdetected" in this case
    // for consistency.
    this.sendAsyncMessage("CookieBanner::DetectedBanner");
    this.sendAsyncMessage("CookieBanner::HandledBanner");
    return true;
  }

  /**
   * Handler for DOMContentLoaded events which is the entry point for cookie
   * banner handling.
   */
  async #onDOMContentLoaded() {
    lazy.logConsole.debug("onDOMContentLoaded", { didLoad: this.#didLoad });
    this.#didLoad = false;

    let principal = this.document?.nodePrincipal;

    // We only apply banner auto-clicking if the document has a content
    // principal.
    if (!principal?.isContentPrincipal) {
      return;
    }

    // We don't need to do auto-clicking if it's not a http/https page.
    if (!principal.schemeIs("http") && !principal.schemeIs("https")) {
      return;
    }

    lazy.logConsole.debug("Send message to get rule", {
      baseDomain: principal.baseDomain,
      isTopLevel: this.browsingContext == this.browsingContext?.top,
    });
    let rules;

    try {
      let data = await this.sendQuery("CookieBanner::GetClickRules", {});

      rules = data.rules;
      // Set we are cooling down for this session if the cookie banner handling
      // has been executed previously.
      this.#isCooledDownInSession = data.hasExecuted;
    } catch (e) {
      lazy.logConsole.warn("Failed to get click rule from parent.", e);
      return;
    }

    lazy.logConsole.debug("Got rules:", rules);
    // We can stop here if we don't have a rule.
    if (!rules.length) {
      // If the cookie injector has handled the banner and there are no click
      // rules we still need to dispatch a "cookiebannerhandled" event.
      this.#dispatchEventsForBannerHandledByInjection();

      this.#maybeSendTestMessage();
      return;
    }

    this.#clickRules = rules;

    let bannerHandled, bannerDetected, matchedRules;
    try {
      ({ bannerHandled, bannerDetected, matchedRules } =
        await this.handleCookieBanner());
    } catch (e) {
      if (DOMException.isInstance(e) && e.name === "AbortError") {
        lazy.logConsole.debug("handleCookieBanner() has aborted");
        return;
      }
      throw e;
    }

    // Send a message to mark that the cookie banner handling has been executed.
    this.sendAsyncMessage("CookieBanner::MarkSiteExecuted");

    let dispatchedEventsForCookieInjection =
      this.#dispatchEventsForBannerHandledByInjection();

    // 1. Detected event.
    if (bannerDetected) {
      lazy.logConsole.info("Detected cookie banner.", {
        url: this.document.location.href,
      });
      // Avoid dispatching a duplicate "cookiebannerdetected" event.
      if (!dispatchedEventsForCookieInjection) {
        this.sendAsyncMessage("CookieBanner::DetectedBanner");
      }
    }

    // 2. Handled event.
    if (bannerHandled) {
      lazy.logConsole.info("Handled cookie banner.", {
        url: this.document.location.href,
        matchedRules,
      });

      // Avoid dispatching a duplicate "cookiebannerhandled" event.
      if (!dispatchedEventsForCookieInjection) {
        this.sendAsyncMessage("CookieBanner::HandledBanner");
      }
    }

    this.#maybeSendTestMessage();
  }

  /**
   * Handler for "load" events. Used as a signal to stop observing the DOM for
   * cookie banners after a timeout.
   */
  #onLoad() {
    this.#didLoad = true;

    // Exit early if we are not handling banners for this site.
    if (!this.#clickRules?.length) {
      return;
    }

    lazy.logConsole.debug("Observed 'load' event", {
      href: this.document.location.href,
      hasActiveObserver: this.#hasActiveObserver,
      observerCleanupTimer: this.#timeoutTimerID,
    });

    // On load reset the timer for cleanup.
    this.#startOrResetCleanupTimer();
  }

  /**
   * We limit how long we observe cookie banner mutations for performance
   * reasons. If not present initially on DOMContentLoaded, cookie banners are
   * expected to show up during or shortly after page load.
   * This method starts a cleanup timeout which duration depends on the current
   * load stage (DOMContentLoaded, or load). When called, if a timeout is
   * already running, it is cancelled and a new timeout is scheduled.
   */
  #startOrResetCleanupTimer() {
    // Cancel any already running timeout so we can schedule a new one.
    if (this.#timeoutTimerID) {
      lazy.logConsole.debug(
        "#startOrResetCleanupTimer: Cancelling existing cleanup timeout",
        {
          didLoad: this.#didLoad,
          id: this.#timeoutTimerID,
        }
      );
      lazy.clearTimeout(this.#timeoutTimerID);
      this.#timeoutTimerID = null;
    }

    let durationMS = this.#didLoad
      ? lazy.cleanupTimeoutAfterLoad
      : lazy.cleanupTimeoutAfterDOMContentLoaded;
    lazy.logConsole.debug(
      "#startOrResetCleanupTimer: Starting cleanup timeout",
      {
        durationMS,
        didLoad: this.#didLoad,
      }
    );

    this.#timeoutTimerID = lazy.setTimeout(() => {
      lazy.logConsole.debug(
        "#startOrResetCleanupTimer: Cleanup timeout triggered",
        {
          durationMS,
          didLoad: this.#didLoad,
        }
      );
      this.#timeoutTimerID = null;
      this.#timeoutSignalController.abort();
    }, durationMS);
  }

  didDestroy() {
    lazy.logConsole.debug("didDestroy() called");

    // Clean up the observer and polling function.
    this.#abortController.abort();
    lazy.clearTimeout(this.#timeoutTimerID);
    this.#timeoutTimerID = null;
  }

  /**
   * The function to perform the core logic of handing the cookie banner. It
   * will detect the banner and click the banner button whenever possible
   * according to the given click rules.
   * If the service mode pref is set to detect only mode we will only attempt to
   * find the cookie banner element and return early.
   *
   * @returns A promise which resolves when it finishes auto clicking.
   */
  async handleCookieBanner() {
    lazy.logConsole.debug("handleCookieBanner", this.document.location.href);

    // Start timer to clean up detection code (polling and mutation observers).
    this.#startOrResetCleanupTimer();

    // First, we detect if the banner is shown on the page
    let rules = await this.#detectBanner();

    if (!rules.length) {
      // The banner was never shown.
      return { bannerHandled: false, bannerDetected: false };
    }

    // No rule with valid button to click. This can happen if we're in
    // MODE_REJECT and there are only opt-in buttons available.
    // This also applies when detect-only mode is enabled. We only want to
    // dispatch events matching the current service mode.
    if (rules.every(rule => rule.target == null)) {
      return { bannerHandled: false, bannerDetected: false };
    }

    // If the cookie banner prefs only enable detection but not handling we're done here.
    if (this.#isDetectOnly) {
      return { bannerHandled: false, bannerDetected: true };
    }

    let successClick = false;
    successClick = await this.#clickTarget(rules);

    return {
      bannerHandled: successClick,
      bannerDetected: true,
      matchedRules: rules,
    };
  }

  /**
   * The helper function to observe the changes on the document with a timeout.
   * It will call the check function when it observes mutations on the document
   * body. Once the check function returns a truthy value, it will resolve with
   * that value. Otherwise, it will resolve with null on timeout.
   *
   * @param {function} [checkFn] - The check function.
   * @returns {Promise} - A promise which resolves with the return value of the
   * check function or null if the function times out.
   */
  #promiseObserve(checkFn) {
    if (this.#hasActiveObserver) {
      throw new Error(
        "The promiseObserve is called before previous one resolves."
      );
    }
    this.#hasActiveObserver = true;

    return new Promise((resolve, reject) => {
      if (this.#abortController.signal.aborted) {
        reject(this.#abortController.signal.reason);
        return;
      }

      if (this.#timeoutSignalController.signal.aborted) {
        resolve(null);
        return;
      }

      let win = this.contentWindow;
      // Marks whether a mutation on the site has been observed since we last
      // ran checkFn.
      let sawMutation = false;

      // IDs for interval for checkFn polling.
      let pollIntervalId = null;

      // Keep track of DOM changes via MutationObserver. We only run query
      // selectors again if the DOM updated since our last check.
      let observer = new win.MutationObserver(() => {
        sawMutation = true;
      });
      observer.observe(win.document.body, {
        attributes: true,
        subtree: true,
        childList: true,
      });

      // Start polling checkFn.
      let intervalFn = () => {
        lazy.logConsole.debug(
          "#promiseObserve interval function",
          this.document.location.href
        );

        if (this.#abortController.signal.aborted) {
          throw new Error(
            "The promiseObserve interval function is still running after banner detection has aborted."
          );
        }

        if (this.#timeoutSignalController.signal.aborted) {
          throw new Error(
            "The promiseObserve interval function is still running after banner detection has timed out."
          );
        }

        // Nothing changed since last run, skip running checkFn.
        if (!sawMutation) {
          return;
        }
        // Reset mutation flag.
        sawMutation = false;

        // A truthy result means we have a hit so we can stop observing.
        let result = checkFn?.();
        if (result) {
          cleanUp();
          resolve(result);
        }
      };
      pollIntervalId = lazy.setInterval(intervalFn, lazy.pollingInterval);

      let cleanUp = () => {
        lazy.logConsole.debug("#promiseObserve cleanup", {
          observer,
          pollIntervalId,
          href: this.document.location?.href,
        });

        // Unregister the observer.
        if (observer) {
          observer.disconnect();
          observer = null;
        }

        // Stop the polling checks.
        if (pollIntervalId) {
          lazy.clearInterval(pollIntervalId);
          pollIntervalId = null;
        }

        this.#hasActiveObserver = false;
        this.#abortController.signal.removeEventListener(
          "abort",
          abortFunction
        );
        this.#timeoutSignalController.signal.removeEventListener(
          "abort",
          timeoutFunction
        );
      };

      let abortFunction = () => {
        cleanUp();
        reject(this.#abortController.signal.reason);
      };
      this.#abortController.signal.addEventListener("abort", abortFunction);

      let timeoutFunction = () => {
        cleanUp();
        resolve(null);
      };
      this.#timeoutSignalController.signal.addEventListener(
        "abort",
        timeoutFunction
      );
    });
  }

  // Detecting if the banner is shown on the page.
  async #detectBanner() {
    if (!this.#clickRules?.length) {
      return [];
    }
    lazy.logConsole.debug("Starting to detect the banner");

    // Returns an array of rules for which a cookie banner exists for the
    // current site.
    let presenceDetector = () => {
      lazy.logConsole.debug("presenceDetector start");
      let matchingRules = this.#clickRules.filter(rule => {
        let { presence, skipPresenceVisibilityCheck } = rule;

        let banner = this.document.querySelector(presence);
        lazy.logConsole.debug("Testing banner el presence", {
          result: banner,
          rule,
          presence,
        });

        if (!banner) {
          return false;
        }

        if (skipPresenceVisibilityCheck) {
          return true;
        }
        return this.#isVisible(banner);
      });

      // For no rules matched return null explicitly so #promiseObserve knows we
      // want to keep observing.
      if (!matchingRules.length) {
        return null;
      }
      return matchingRules;
    };

    lazy.logConsole.debug("Initial call to presenceDetector");
    let rules = presenceDetector();

    // If we couldn't detect the banner at the beginning, we register an
    // observer with the timeout to observe if the banner was shown within the
    // timeout.
    if (!rules?.length) {
      lazy.logConsole.debug(
        "Initial presenceDetector failed, registering MutationObserver",
        rules
      );
      rules = await this.#promiseObserve(presenceDetector);
    }

    if (!rules?.length) {
      lazy.logConsole.debug("Couldn't detect the banner", rules);
      return [];
    }

    lazy.logConsole.debug("Detected the banner for rules", rules);

    return rules;
  }

  // Clicking the target button.
  async #clickTarget(rules) {
    lazy.logConsole.debug("Starting to detect the target button");

    let targetEl;
    for (let rule of rules) {
      targetEl = this.document.querySelector(rule.target);
      if (targetEl) {
        break;
      }
    }

    // The target button is not available. We register an observer to wait until
    // it's ready.
    if (!targetEl) {
      targetEl = await this.#promiseObserve(() => {
        for (let rule of rules) {
          let el = this.document.querySelector(rule.target);

          lazy.logConsole.debug("Testing button el presence", {
            result: el,
            rule,
            target: rule.target,
          });

          if (el) {
            lazy.logConsole.debug(
              "Found button from rule",
              rule,
              rule.target,
              el
            );
            return el;
          }
        }
        return null;
      });

      if (!targetEl) {
        lazy.logConsole.debug("Cannot find the target button.");
        return false;
      }
    }

    lazy.logConsole.debug("Found the target button, click it.", targetEl);
    targetEl.click();
    return true;
  }

  // The helper function to check if the given element if visible.
  #isVisible(element) {
    return element.checkVisibility({
      checkOpacity: true,
      checkVisibilityCSS: true,
    });
  }

  #maybeSendTestMessage() {
    if (lazy.testing) {
      let win = this.contentWindow;

      // Report the clicking is finished after the style has been flushed.
      win.requestAnimationFrame(() => {
        win.setTimeout(() => {
          this.sendAsyncMessage("CookieBanner::Test-FinishClicking");
        }, 0);
      });
    }
  }
}
