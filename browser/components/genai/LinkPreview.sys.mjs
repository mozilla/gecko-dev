/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  LinkPreviewModel:
    "moz-src:///browser/components/genai/LinkPreviewModel.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PrefUtils: "resource://normandy/lib/PrefUtils.sys.mjs",
  Region: "resource://gre/modules/Region.sys.mjs",
});

export const LABS_STATE = Object.freeze({
  NOT_ENROLLED: 0,
  ENROLLED: 1,
  ROLLOUT_ENDED: 2,
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "allowedLanguages",
  "browser.ml.linkPreview.allowedLanguages"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "collapsed",
  "browser.ml.linkPreview.collapsed",
  null,
  (_pref, _old, val) => LinkPreview.onCollapsedPref(val)
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "enabled",
  "browser.ml.linkPreview.enabled",
  null,
  (_pref, _old, val) => LinkPreview.onEnabledPref(val)
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "ignoreMs",
  "browser.ml.linkPreview.ignoreMs"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "labs",
  "browser.ml.linkPreview.labs",
  LABS_STATE.NOT_ENROLLED
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "longPress",
  "browser.ml.linkPreview.longPress"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "longPressMs",
  "browser.ml.linkPreview.longPressMs"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "nimbus",
  "browser.ml.linkPreview.nimbus"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "noKeyPointsRegions",
  "browser.ml.linkPreview.noKeyPointsRegions"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "onboardingCooldownPeriodMs",
  "browser.ml.linkPreview.onboardingCooldownPeriodMs",
  7 * 24 * 60 * 60 * 1000 // Constant for onboarding reactivation cooldown period (7 days in milliseconds)
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "onboardingHoverLinkMs",
  "browser.ml.linkPreview.onboardingHoverLinkMs",
  500
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "onboardingMaxShowFreq",
  "browser.ml.linkPreview.onboardingMaxShowFreq",
  2
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "onboardingTimes",
  "browser.ml.linkPreview.onboardingTimes",
  "", // default (when PREF_INVALID)
  null, // no onUpdate callback
  rawValue => {
    if (!rawValue) {
      return [];
    }
    return rawValue.split(",").map(Number);
  }
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "optin",
  "browser.ml.linkPreview.optin",
  null,
  (_pref, _old, val) => LinkPreview.onOptinPref(val)
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "prefetchOnEnable",
  "browser.ml.linkPreview.prefetchOnEnable",
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "recentTypingMs",
  "browser.ml.linkPreview.recentTypingMs"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "shift",
  "browser.ml.linkPreview.shift"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "shiftAlt",
  "browser.ml.linkPreview.shiftAlt"
);

export const LinkPreview = {
  // Shared downloading state to use across multiple previews
  progress: -1, // -1 = off, 0-100 = download progress

  cancelLongPress: null,
  keyboardComboActive: false,
  overLinkTime: 0,
  recentTyping: 0,
  _windowStates: new Map(),
  linkPreviewPanelId: "link-preview-panel",

  get canShowKeyPoints() {
    return this._isRegionSupported();
  },

  get canShowLegacy() {
    return lazy.labs != LABS_STATE.NOT_ENROLLED;
  },

  get canShowPreferences() {
    return lazy.enabled;
  },

  get showOnboarding() {
    const timesArray = lazy.onboardingTimes;

    const lastValidTime = timesArray.at(-1) || 0;
    const timeSinceLastOnboarding = Date.now() - lastValidTime;

    return (
      timesArray.length < lazy.onboardingMaxShowFreq &&
      timeSinceLastOnboarding >= lazy.onboardingCooldownPeriodMs
    );
  },

  shouldShowContextMenu(nsContextMenu) {
    // In a future patch, we can further analyze the link, etc.
    //link url value: nsContextMenu.linkURL
    // For now, let’s rely on whether LinkPreview is enabled and region supported
    //link conditions are borrowed from context-stripOnShareLink

    return (
      this._isRegionSupported() &&
      lazy.enabled &&
      (nsContextMenu.onLink || nsContextMenu.onPlainTextLink) &&
      !nsContextMenu.onMailtoLink &&
      !nsContextMenu.onTelLink &&
      !nsContextMenu.onMozExtLink
    );
  },

  /**
   * Handles the preference change for enabling/disabling Link Preview.
   * It adds or removes event listeners for all tracked windows based on the new preference value.
   *
   * @param {boolean} enabled - The new state of the Link Preview preference.
   */
  onEnabledPref(enabled) {
    const method = enabled ? "_addEventListeners" : "_removeEventListeners";
    for (const win of this._windowStates.keys()) {
      this[method](win);
    }

    // Prefetch the model when enabling by simulating a request.
    if (enabled && lazy.prefetchOnEnable && this._isRegionSupported()) {
      this.generateKeyPoints();
    }

    Glean.genaiLinkpreview.enabled.set(enabled);
    Glean.genaiLinkpreview.labsCheckbox.record({ enabled });

    this.handleNimbusPrefs();
  },

  /**
   * Updates a property on the link-preview-card element for all window states.
   *
   * @param {string} prop - The property to update.
   * @param {*} value - The value to set for the property.
   */
  updateCardProperty(prop, value) {
    for (const [win] of this._windowStates) {
      const panel = win.document.getElementById(this.linkPreviewPanelId);
      if (!panel) {
        continue;
      }

      const card = panel.querySelector("link-preview-card");
      if (card) {
        card[prop] = value;
      }
    }
  },

  /**
   * Handles the preference change for opt-in state.
   * Updates all link preview cards with the new opt-in state.
   *
   * @param {boolean} optin - The new state of the opt-in preference.
   */
  onOptinPref(optin) {
    this.updateCardProperty("optin", optin);
    Glean.genaiLinkpreview.cardAiConsent.record({
      option: optin ? "continue" : "cancel",
    });
  },

  /**
   * Handles the preference change for collapsed state.
   * Updates all link preview cards with the new collapsed state.
   *
   * @param {boolean} collapsed - The new state of the collapsed preference.
   */
  onCollapsedPref(collapsed) {
    this.updateCardProperty("collapsed", collapsed);
    Glean.genaiLinkpreview.keyPointsToggle.record({ expand: !collapsed });
  },

  /**
   * Handles Nimbus preferences, e.g., migrating, restoring, setting.
   */
  handleNimbusPrefs() {
    // For those who turned on via labs with enabled setPref variable, persist
    // the pref and allow using shift-alt matching labs copy.
    if (
      lazy.NimbusFeatures.linkPreviews.getVariable("enabled") &&
      lazy.labs == LABS_STATE.NOT_ENROLLED
    ) {
      Services.prefs.setIntPref(
        "browser.ml.linkPreview.labs",
        LABS_STATE.ENROLLED
      );
      Services.prefs.setBoolPref("browser.ml.linkPreview.shiftAlt", true);
    }
    // Restore pref once if previously enabled via labs assuming rollout ended.
    else if (!lazy.enabled && lazy.labs == LABS_STATE.ENROLLED) {
      Services.prefs.setIntPref(
        "browser.ml.linkPreview.labs",
        LABS_STATE.ROLLOUT_ENDED
      );
      Services.prefs.setBoolPref("browser.ml.linkPreview.enabled", true);
    }

    // Handle nimbus feature pref setting
    if (this._nimbusRegistered) {
      return;
    }
    this._nimbusRegistered = true;
    const featureId = "linkPreviews";
    lazy.NimbusFeatures[featureId].onUpdate(() => {
      const enrollment = lazy.NimbusFeatures[featureId].getEnrollmentMetadata();
      if (!enrollment) {
        return;
      }

      // Set prefs on any branch if we have a new enrollment slug, otherwise
      // only set default branch as those only last for the session
      const slug = enrollment.slug + ":" + enrollment.branch;
      const anyBranch = slug != lazy.nimbus;
      const setPref = ([pref, { branch = "user", value = null }]) => {
        if (anyBranch || branch == "default") {
          lazy.PrefUtils.setPref("browser.ml.linkPreview." + pref, value, {
            branch,
          });
        }
      };
      setPref(["nimbus", { value: slug }]);
      Object.entries(
        lazy.NimbusFeatures[featureId].getVariable("prefs") ?? []
      ).forEach(setPref);
    });
  },

  /**
   * Handles startup tasks such as telemetry and adding listeners.
   *
   * @param {Window} win - The window context used to add event listeners.
   */
  init(win) {
    // Access getters for side effects of observing pref changes
    lazy.collapsed;
    lazy.enabled;
    lazy.optin;

    this._windowStates.set(win, {});
    if (!win.customElements.get("link-preview-card")) {
      win.ChromeUtils.importESModule(
        "chrome://browser/content/genai/content/link-preview-card.mjs",
        { global: "current" }
      );
    }
    if (!win.customElements.get("link-preview-card-onboarding")) {
      win.ChromeUtils.importESModule(
        "chrome://browser/content/genai/content/link-preview-card-onboarding.mjs",
        { global: "current" }
      );
    }

    this.handleNimbusPrefs();

    if (lazy.enabled) {
      this._addEventListeners(win);
    }

    Glean.genaiLinkpreview.enabled.set(lazy.enabled);
  },

  /**
   * Teardown the Link Preview feature for the given window.
   * Removes event listeners from the specified window and removes it from the window map.
   *
   * @param {Window} win - The window context to uninitialize.
   */
  teardown(win) {
    // Remove event listeners from the specified window
    if (lazy.enabled) {
      this._removeEventListeners(win);
    }

    // Remove the panel if it exists
    const doc = win.document;
    doc.getElementById(this.linkPreviewPanelId)?.remove();

    // Remove the window from the map
    this._windowStates.delete(win);
  },

  /**
   * Adds all needed event listeners and updates the state.
   *
   * @param {Window} win - The window to which event listeners are added.
   */
  _addEventListeners(win) {
    win.addEventListener("OverLink", this, true);
    win.addEventListener("keydown", this, true);
    win.addEventListener("keyup", this, true);
    win.addEventListener("mousedown", this, true);
  },

  /**
   * Removes all event listeners and updates the state.
   *
   * @param {Window} win - The window from which event listeners are removed.
   */
  _removeEventListeners(win) {
    win.removeEventListener("OverLink", this, true);
    win.removeEventListener("keydown", this, true);
    win.removeEventListener("keyup", this, true);
    win.removeEventListener("mousedown", this, true);

    // Long press might have added listeners to this window.
    this.cancelLongPress?.();
  },

  /**
   * Handles keyboard events ("keydown" and "keyup") for the Link Preview feature.
   * Adjusts the state of keyboardComboActive based on modifier keys.
   *
   * @param {KeyboardEvent} event - The keyboard event to be processed.
   */
  handleEvent(event) {
    switch (event.type) {
      case "keydown":
      case "keyup":
        this._onKeyEvent(event);
        break;
      case "OverLink":
        this._onLinkPreview(event);
        break;
      case "dragstart":
      case "mousedown":
      case "mouseup":
        this._onPressEvent(event);
        break;
      default:
        break;
    }
  },

  /**
   * Handles "keydown" and "keyup" events.
   *
   * @param {KeyboardEvent} event - The keyboard event to be processed.
   */
  _onKeyEvent(event) {
    const win = event.currentTarget;

    // Track regular typing to suppress keyboard previews.
    if (event.key.length == 1 || ["Enter", "Tab"].includes(event.key)) {
      this.recentTyping = Date.now();
    }

    // Keyboard combos requires shift and neither ctrl nor meta.
    this.keyboardComboActive = false;
    if (!event.shiftKey || event.ctrlKey || event.metaKey) {
      return;
    }

    // Handle shift without alt if preference is set.
    if (!event.altKey && lazy.shift) {
      this.keyboardComboActive = "shift";
    }
    // Handle shift with alt if preference is set.
    else if (event.altKey && lazy.shiftAlt) {
      this.keyboardComboActive = "shift-alt";
    }
    // New presses or releases can result in desired combo for previewing.
    this._maybeLinkPreview(win);
  },

  /**
   * Handles "OverLink" events.
   * Stores the hovered link URL in the per-window state object and processes the
   * link preview if the keyboard combination is active.
   *
   * @param {CustomEvent} event - The event object containing details about the link preview.
   */
  _onLinkPreview(event) {
    const win = event.currentTarget;
    const url = event.detail.url;

    // Store the current overLink in the per-window state object filtering out
    // links common for dynamic single page apps.
    const stateObject = this._windowStates.get(win);
    stateObject.overLink =
      url.endsWith("#") || url.startsWith("javascript:") ? "" : url;
    this.overLinkTime = Date.now();

    // If the keyboard combo is active, always check for link preview
    // regardless of whether it's the same URL.
    if (this.keyboardComboActive) {
      this._maybeLinkPreview(win);
    } else if (this.showOnboarding) {
      this._maybeOnboard(win, url, stateObject);
    }
  },

  _maybeOnboard(win, url, stateObject) {
    if (!url) {
      return;
    }

    const panel = win.document.getElementById(this.linkPreviewPanelId);
    const isPanelOpen = panel && panel.state !== "closed";

    // If panel is open or it's the same URL as last hover, don't start
    // hover-based onboarding timer.
    if (isPanelOpen || url === stateObject.lastHoveredUrl) {
      return;
    }

    // Clear any existing timer when moving to a new link
    if (stateObject.hoverTimerId) {
      win.clearTimeout(stateObject.hoverTimerId);
      stateObject.hoverTimerId = null;
    }

    // Update last hovered URL
    stateObject.lastHoveredUrl = url;
    stateObject.hoverTimerId = win.setTimeout(() => {
      // Only show if we're still hovering the same URL
      if (stateObject.overLink === url) {
        this.renderOnboardingPanel(win, url);
      }
      stateObject.lastHoveredUrl = "";
      stateObject.hoverTimerId = null;
    }, lazy.onboardingHoverLinkMs);
  },

  /**
   * Renders the onboarding panel for link preview.
   * Updates onboardingTimes and renders onboarding card
   *
   * @param {Window} win - The browser window context.
   * @param {string} url - The URL of the link to be previewed.
   */
  async renderOnboardingPanel(win, url) {
    // Append the current time to onboarding times.
    Services.prefs.setStringPref("browser.ml.linkPreview.onboardingTimes", [
      ...lazy.onboardingTimes,
      Date.now(),
    ]);

    // Telemetry for onboarding card view
    Glean.genaiLinkpreview.onboardingCard.record({ action: "view" });

    // Now show the preview as an "onboarding" source
    const panel = this.initOrResetPreviewPanel(win, "onboarding");

    const doc = win.document;
    const onboardingCard = doc.createElement("link-preview-card-onboarding");
    onboardingCard.style.width = "100%";
    onboardingCard.addEventListener(
      "LinkPreviewCard:onboardingComplete",
      () => {
        Glean.genaiLinkpreview.onboardingCard.record({
          action: "try_it_now",
        });
        this.renderLinkPreviewPanel(win, url, "onboarding");
      }
    );
    onboardingCard.addEventListener("LinkPreviewCard:onboardingClose", () => {
      panel.hidePopup();
    });

    panel.append(onboardingCard);
    panel.openPopupNearMouse();
  },

  /**
   * Initializes a new link preview panel or resets an existing one.
   * Ensures the panel is ready to display content.
   *
   * @param {Window} win - The browser window context.
   * @param {string} cardType - The trigger source for the panel initialization
   * @returns {Panel} The initialized or reset panel element.
   */
  initOrResetPreviewPanel(win, cardType) {
    const doc = win.document;
    let panel = doc.getElementById(this.linkPreviewPanelId);

    // If it already exists, hide any open popup and clear out old content.
    if (panel) {
      // Transitioning from onboarding reuses the panel without hiding.
      if (panel.cardType == "linkpreview") {
        panel.hidePopup();
      }
      panel.replaceChildren();
    } else {
      panel = doc
        .getElementById("mainPopupSet")
        .appendChild(doc.createXULElement("panel"));
      panel.className = "panel-no-padding";
      panel.id = this.linkPreviewPanelId;
      panel.setAttribute("noautofocus", true);
      panel.setAttribute("type", "arrow");
      panel.style.width = "362px";
      panel.style.setProperty("--og-padding", "var(--space-xlarge)");
      // Match the radius of the image extended out by the padding.
      panel.style.setProperty(
        "--panel-border-radius",
        "calc(var(--border-radius-small) + var(--og-padding))"
      );

      const openPopup = () => {
        const { _x: x, _y: y } = win.MousePosTracker;
        // Open near the mouse offsetting so link in the card can be clicked.
        panel.openPopup(doc.documentElement, "overlap", x - 20, y - 160);
        panel.openTime = Date.now();
      };
      panel.openPopupNearMouse = openPopup;

      // Add a single, unified popuphidden listener once on panel init. This
      // listener will check panel.cardType to determine the correct Glean call.
      panel.addEventListener("popuphidden", () => {
        if (panel.cardType === "onboarding") {
          Glean.genaiLinkpreview.onboardingCard.record({
            action: "close",
          });
        } else if (panel.cardType === "linkpreview") {
          Glean.genaiLinkpreview.cardClose.record({
            duration: Date.now() - panel.openTime,
          });
        }
      });
    }
    panel.cardType = cardType;
    return panel;
  },

  /**
   * Handles long press events.
   *
   * @param {MouseEvent} event - The mouse related events to be processed.
   */
  _onPressEvent(event) {
    if (!lazy.longPress) {
      return;
    }

    // Check for the start of a long unmodified primary button press on a link.
    const win = event.currentTarget;
    const stateObject = this._windowStates.get(win);
    if (
      event.type == "mousedown" &&
      !event.button &&
      !event.altKey &&
      !event.ctrlKey &&
      !event.metaKey &&
      !event.shiftKey &&
      stateObject.overLink
    ) {
      // Detect events to cancel the long press.
      win.addEventListener("dragstart", this, true);
      win.addEventListener("mouseup", this, true);

      // Show preview after a delay if not cancelled.
      const timer = win.setTimeout(() => {
        this.cancelLongPress();
        this.renderLinkPreviewPanel(win, stateObject.overLink, "longpress");
      }, lazy.longPressMs);

      // Provide a way to clean up.
      this.cancelLongPress = () => {
        win.clearTimeout(timer);
        win.removeEventListener("dragstart", this, true);
        win.removeEventListener("mouseup", this, true);
        this.cancelLongPress = null;
      };
    } else {
      this.cancelLongPress?.();
    }
  },

  /**
   * Checks if the user's region is supported for key points generation.
   *
   * @returns {boolean} True if the region is supported, false otherwise.
   */
  _isRegionSupported() {
    const disallowedRegions = lazy.noKeyPointsRegions
      .split(",")
      .map(region => region.trim().toUpperCase());

    const userRegion = lazy.Region.home?.toUpperCase();
    return !disallowedRegions.includes(userRegion);
  },

  /**
   * Creates an Open Graph (OG) card using meta information from the page.
   *
   * @param {Document} doc - The document object where the OG card will be
   * created.
   * @param {object} pageData - An object containing page data, including meta
   * tags and article information.
   * @param {object} [pageData.article] - Optional article-specific data.
   * @param {object} [pageData.metaInfo] - Optional meta tag key-value pairs.
   * @returns {Element} A DOM element representing the OG card.
   */
  createOGCard(doc, pageData) {
    const ogCard = doc.createElement("link-preview-card");
    ogCard.style.width = "100%";
    ogCard.pageData = pageData;

    ogCard.optin = lazy.optin;
    ogCard.collapsed = lazy.collapsed;

    // Reflect the shared download progress to this preview.
    const updateProgress = () => {
      ogCard.progress = this.progress;
      // If we are still downloading, update the progress again.
      if (this.progress >= 0) {
        doc.ownerGlobal.setTimeout(
          () => ogCard.isConnected && updateProgress(),
          250
        );
      }
    };
    updateProgress();

    if (!this._isRegionSupported()) {
      // Region not supported, just don't show key points section
      return ogCard;
    }

    // Generate key points if we have content, language and configured for any
    // language or restricted.
    if (
      pageData.article.textContent &&
      pageData.article.detectedLanguage &&
      (!lazy.allowedLanguages ||
        lazy.allowedLanguages
          .split(",")
          .includes(pageData.article.detectedLanguage))
    ) {
      this.generateKeyPoints(ogCard);
    } else {
      ogCard.isMissingDataErrorState = true;
    }

    return ogCard;
  },

  /**
   * Generate AI key points for card.
   *
   * @param {LinkPreviewCard} ogCard to add key points
   * @param {boolean} _retry Indicates whether to retry the operation.
   */
  async generateKeyPoints(ogCard, _retry = false) {
    // Prevent keypoints if user not opt-in to link preview or user is set
    // keypoints to be collapsed.
    if (!lazy.optin || lazy.collapsed) {
      return;
    }

    // Support prefetching without a card by mocking expected properties.
    let outcome = ogCard ? "success" : "prefetch";
    if (!ogCard) {
      ogCard = { addKeyPoint() {}, isConnected: true, keyPoints: [] };
    }

    const startTime = Date.now();
    ogCard.generating = true;

    // Ensure sequential AI processing to reduce memory usage by passing our
    // promise to the next request before waiting on the previous.
    const previous = this.lastRequest;
    const { promise, resolve } = Promise.withResolvers();
    this.lastRequest = promise;
    await previous;
    const delay = Date.now() - startTime;

    // No need to generate if already removed.
    if (!ogCard.isConnected) {
      resolve();
      Glean.genaiLinkpreview.generate.record({
        delay,
        outcome: "removed",
      });
      return;
    }

    let download, latency;
    try {
      await lazy.LinkPreviewModel.generateTextAI(
        ogCard.pageData?.article.textContent ?? "",
        {
          onDownload: (downloading, percentage) => {
            // Initial percentage is NaN, so set to 0.
            percentage = isNaN(percentage) ? 0 : percentage;
            // Use the percentage while downloading, otherwise disable with -1.
            this.progress = downloading ? percentage : -1;
            ogCard.progress = this.progress;
            download = Date.now() - startTime;
          },
          onError: error => {
            console.error(error);
            outcome = error;
            ogCard.generationError = error;
          },
          onText: text => {
            // Clear waiting in case a different generate handled download.
            ogCard.showWait = false;
            ogCard.addKeyPoint(text);
            latency = latency ?? Date.now() - startTime;
          },
        }
      );
    } finally {
      resolve();
      ogCard.generating = false;
      Glean.genaiLinkpreview.generate.record({
        delay,
        download,
        latency,
        outcome,
        sentences: ogCard.keyPoints.length,
        time: Date.now() - startTime,
      });
    }
  },

  /**
   * Handles key points generation requests from different user actions.
   * This is a shared handler for both retry and initial generation events.
   * Resets error states and triggers key points generation.
   *
   * @param {LinkPreviewCard} ogCard - The card element to generate key points for
   * @private
   */
  _handleKeyPointsGenerationEvent(ogCard) {
    // Reset error states
    ogCard.isMissingDataErrorState = false;
    ogCard.isGenerationErrorState = false;

    this.generateKeyPoints(ogCard, true);
  },

  /**
   * Renders the link preview panel at the specified coordinates.
   *
   * @param {Window} win - The browser window context.
   * @param {string} url - The URL of the link to be previewed.
   * @param {string} source - Optional trigging behavior.
   */
  async renderLinkPreviewPanel(win, url, source = "shortcut") {
    // If link preview is used once not via onboarding, stop onboarding.
    if (source !== "onboarding") {
      const maxFreq = lazy.onboardingMaxShowFreq;
      // Fill the times array up to maxFreq with an array of 0 timestamps.
      Services.prefs.setStringPref(
        "browser.ml.linkPreview.onboardingTimes",
        [...lazy.onboardingTimes, ...Array(maxFreq).fill("0")].slice(0, maxFreq)
      );
    }

    // Transition from onboarding to preview content with transparency.
    const doc = win.document;
    let panel = doc.getElementById(this.linkPreviewPanelId);
    if (source == "onboarding") {
      panel.style.setProperty("opacity", "0");
    }

    // Reuse or initialize panel.
    if (panel && panel.previewUrl == url) {
      if (panel.state == "closed") {
        panel.openPopupNearMouse();
        Glean.genaiLinkpreview.start.record({ cached: true, source });
      }
      return;
    }
    panel = this.initOrResetPreviewPanel(win, "linkpreview");
    panel.previewUrl = url;

    Glean.genaiLinkpreview.start.record({ cached: false, source });

    // TODO we want to immediately add a card as a placeholder to have UI be
    // more responsive while we wait on fetching page data.
    const browsingContext = win.browsingContext;
    const actor = browsingContext.currentWindowGlobal.getActor("LinkPreview");
    const fetchTime = Date.now();
    const pageData = await actor.fetchPageData(url);
    // Skip updating content if we've moved on to showing something else.
    const skipped = pageData.url != panel.previewUrl;
    Glean.genaiLinkpreview.fetch.record({
      description: !!pageData.meta.description,
      image: !!pageData.meta.imageUrl,
      length:
        Math.round((pageData.article.textContent?.length ?? 0) * 0.01) * 100,
      outcome: pageData.error?.result ?? "success",
      sitename: !!pageData.article.siteName,
      skipped,
      time: Date.now() - fetchTime,
      title: !!pageData.meta.title,
    });
    if (skipped) {
      return;
    }

    const ogCard = this.createOGCard(doc, pageData);
    panel.append(ogCard);
    ogCard.addEventListener("LinkPreviewCard:dismiss", event => {
      panel.hidePopup();
      Glean.genaiLinkpreview.cardLink.record({ source: event.detail });
    });

    ogCard.addEventListener("LinkPreviewCard:retry", _event => {
      this._handleKeyPointsGenerationEvent(ogCard, "retry");
      Glean.genaiLinkpreview.cardLink.record({ source: "retry" });
    });

    ogCard.addEventListener("LinkPreviewCard:generate", _event => {
      if (ogCard.keyPoints?.length || ogCard.generating) {
        return;
      }
      this._handleKeyPointsGenerationEvent(ogCard, "generate");
    });

    // Make sure panel is visible if previously showing onboarding.
    panel.style.setProperty("opacity", "1");
    if (source !== "onboarding") {
      panel.openPopupNearMouse();
    }
  },

  /**
   * Determines whether to process or cancel the link preview based on the current state.
   * If a URL is available and the keyboard combination is active, it processes the link preview.
   * Otherwise, it cancels the link preview.
   *
   * @param {Window} win - The window context in which the link preview may occur.
   */
  _maybeLinkPreview(win) {
    const stateObject = this._windowStates.get(win);
    const url = stateObject.overLink;
    // Render preview if we have url, keyboard combo and not recently typing.
    // Ignore check intends to avoid cases where mouse happens to be over a
    // link, e.g., after navigating then using an in-page keyboard shortcut or
    // typing characters that require shift.
    if (
      url &&
      this.keyboardComboActive &&
      Date.now() - this.overLinkTime <= lazy.ignoreMs &&
      Date.now() - this.recentTyping >= lazy.recentTypingMs
    ) {
      this.renderLinkPreviewPanel(win, url, this.keyboardComboActive);
    }
  },

  /**
   * Handles the link preview context menu click using the provided URL
   * and nsContextMenu, prompting the link preview panel to open.
   *
   * @param {string} url - The URL of the link to be previewed.
   * @param {object} nsContextMenu - The context menu object containing browser information.
   */
  async handleContextMenuClick(url, nsContextMenu) {
    let win = nsContextMenu.browser.ownerGlobal;
    this.renderLinkPreviewPanel(win, url, "context");
  },
};
