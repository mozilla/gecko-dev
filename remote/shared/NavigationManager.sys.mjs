/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { EventEmitter } from "resource://gre/modules/EventEmitter.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowsingContextListener:
    "chrome://remote/content/shared/listeners/BrowsingContextListener.sys.mjs",
  generateUUID: "chrome://remote/content/shared/UUID.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  PromptListener:
    "chrome://remote/content/shared/listeners/PromptListener.sys.mjs",
  registerNavigationListenerActor:
    "chrome://remote/content/shared/js-window-actors/NavigationListenerActor.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  truncate: "chrome://remote/content/shared/Format.sys.mjs",
  unregisterNavigationListenerActor:
    "chrome://remote/content/shared/js-window-actors/NavigationListenerActor.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

/**
 * @typedef {object} BrowsingContextDetails
 * @property {string} browsingContextId - The browsing context id.
 * @property {string} browserId - The id of the Browser owning the browsing
 *     context.
 * @property {BrowsingContext=} context - The BrowsingContext itself, if
 *     available.
 * @property {boolean} isTopBrowsingContext - Whether the browsing context is
 *     top level.
 */

/**
 * @typedef {object} NavigationInfo
 * @property {'registered'|'started'|'finished'} state - The navigation state.
 * @property {string} navigationId - The UUID for the navigation.
 * @property {string} navigable - The UUID for the navigable.
 * @property {string} url - The target url for the navigation.
 */

/**
 * The NavigationRegistry is responsible for monitoring all navigations happening
 * in the browser.
 *
 * It relies on a JSWindowActor pair called NavigationListener{Parent|Child},
 * found under remote/shared/js-window-actors. As a simple overview, the
 * NavigationListenerChild will monitor navigations in all window globals using
 * content process WebProgressListener, and will forward each relevant update to
 * the NavigationListenerParent
 *
 * The NavigationRegistry singleton holds the map of navigations, from navigable
 * to NavigationInfo. It will also be called by NavigationListenerParent
 * whenever a navigation event happens.
 *
 * This singleton is not exported outside of this class, and consumers instead
 * need to use the NavigationManager class. The NavigationRegistry keeps track
 * of how many NavigationListener instances are currently listening in order to
 * know if the NavigationListenerActor should be registered or not.
 *
 * The NavigationRegistry exposes an API to retrieve the current or last
 * navigation for a given navigable, and also forwards events to notify about
 * navigation updates to individual NavigationManager instances.
 *
 * @class NavigationRegistry
 */
class NavigationRegistry extends EventEmitter {
  #contextListener;
  #managers;
  #navigations;
  #promptListener;

  constructor() {
    super();

    // Set of NavigationManager instances currently used.
    this.#managers = new Set();

    // Maps navigable id to NavigationInfo.
    this.#navigations = new Map();

    this.#contextListener = new lazy.BrowsingContextListener();
    this.#contextListener.on("discarded", this.#onContextDiscarded);

    this.#promptListener = new lazy.PromptListener();
    this.#promptListener.on("closed", this.#onPromptClosed);
    this.#promptListener.on("opened", this.#onPromptOpened);
  }

  /**
   * Retrieve the last known navigation data for a given browsing context.
   *
   * @param {BrowsingContext} context
   *     The browsing context for which the navigation event was recorded.
   * @returns {NavigationInfo|null}
   *     The last known navigation data, or null.
   */
  getNavigationForBrowsingContext(context) {
    if (!lazy.TabManager.isValidCanonicalBrowsingContext(context)) {
      // Bail out if the provided context is not a valid CanonicalBrowsingContext
      // instance.
      return null;
    }

    const navigableId = lazy.TabManager.getIdForBrowsingContext(context);
    if (!this.#navigations.has(navigableId)) {
      return null;
    }

    return this.#navigations.get(navigableId);
  }

  /**
   * Start monitoring navigations in all browsing contexts. This will register
   * the NavigationListener JSWindowActor and will initialize them in all
   * existing browsing contexts.
   */
  startMonitoring(listener) {
    if (this.#managers.size == 0) {
      lazy.registerNavigationListenerActor();
      this.#contextListener.startListening();
      this.#promptListener.startListening();
    }

    this.#managers.add(listener);
  }

  /**
   * Stop monitoring navigations. This will unregister the NavigationListener
   * JSWindowActor and clear the information collected about navigations so far.
   */
  stopMonitoring(listener) {
    if (!this.#managers.has(listener)) {
      return;
    }

    this.#managers.delete(listener);
    if (this.#managers.size == 0) {
      this.#contextListener.stopListening();
      this.#promptListener.stopListening();
      lazy.unregisterNavigationListenerActor();
      // Clear the map.
      this.#navigations = new Map();
    }
  }

  /**
   * Called when a fragment navigation is recorded from the
   * NavigationListener actors.
   *
   * This entry point is only intended to be called from
   * NavigationListenerParent, to avoid setting up observers or listeners,
   * which are unnecessary since NavigationManager has to be a singleton.
   *
   * @param {object} data
   * @param {BrowsingContext} data.context
   *     The browsing context for which the navigation event was recorded.
   * @param {string} data.url
   *     The URL as string for the navigation.
   * @returns {NavigationInfo}
   *     The navigation created for this hash changed navigation.
   */
  notifyFragmentNavigated(data) {
    const { contextDetails, url } = data;

    const context = this.#getContextFromContextDetails(contextDetails);
    const navigableId = lazy.TabManager.getIdForBrowsingContext(context);

    const navigationId = this.#getOrCreateNavigationId(navigableId);
    const navigation = { state: "finished", navigationId, url };
    this.#navigations.set(navigableId, navigation);

    // Hash change navigations are immediately done, fire a single event.
    this.emit("fragment-navigated", { navigationId, navigableId, url });

    return navigation;
  }
  /**
   * Called when a same-document navigation is recorded from the
   * NavigationListener actors.
   *
   * This entry point is only intended to be called from
   * NavigationListenerParent, to avoid setting up observers or listeners,
   * which are unnecessary since NavigationManager has to be a singleton.
   *
   * @param {object} data
   * @param {BrowsingContext} data.context
   *     The browsing context for which the navigation event was recorded.
   * @param {string} data.url
   *     The URL as string for the navigation.
   * @returns {NavigationInfo}
   *     The navigation created for this same-document navigation.
   */
  notifySameDocumentChanged(data) {
    const { contextDetails, url } = data;

    const context = this.#getContextFromContextDetails(contextDetails);
    const navigableId = lazy.TabManager.getIdForBrowsingContext(context);

    const navigationId = this.#getOrCreateNavigationId(navigableId);
    const navigation = { state: "finished", navigationId, url };
    this.#navigations.set(navigableId, navigation);

    // Same document navigations are immediately done, fire a single event.

    this.emit("same-document-changed", { navigationId, navigableId, url });

    return navigation;
  }

  /**
   * Called when a navigation-failed event is recorded from the
   * NavigationListener actors.
   *
   * This entry point is only intended to be called from
   * NavigationListenerParent, to avoid setting up observers or listeners,
   * which are unnecessary since NavigationManager has to be a singleton.
   *
   * @param {object} data
   * @param {BrowsingContextDetails} data.contextDetails
   *     The details about the browsing context for this navigation.
   * @param {string} data.errorName
   *     The error message.
   * @param {string} data.url
   *     The URL as string for the navigation.
   * @returns {NavigationInfo}
   *     The created navigation or the ongoing navigation, if applicable.
   */
  notifyNavigationFailed(data) {
    const { contextDetails, errorName, url } = data;

    const context = this.#getContextFromContextDetails(contextDetails);
    const navigableId = lazy.TabManager.getIdForBrowsingContext(context);

    const navigation = this.#navigations.get(navigableId);

    if (!navigation) {
      lazy.logger.trace(
        lazy.truncate`[${navigableId}] No navigation found to fail for url: ${url}`
      );
      return null;
    }

    if (navigation.state === "finished") {
      lazy.logger.trace(
        `[${navigableId}] Navigation already marked as finished, navigationId: ${navigation.navigationId}`
      );
      return navigation;
    }

    lazy.logger.trace(
      lazy.truncate`[${navigableId}] Navigation failed for url: ${url} (${navigation.navigationId})`
    );

    navigation.state = "finished";

    this.emit("navigation-failed", {
      contextId: context.id,
      errorName,
      navigationId: navigation.navigationId,
      navigableId,
      url,
    });

    return navigation;
  }

  /**
   * Called when a navigation-started event is recorded from the
   * NavigationListener actors.
   *
   * This entry point is only intended to be called from
   * NavigationListenerParent, to avoid setting up observers or listeners,
   * which are unnecessary since NavigationManager has to be a singleton.
   *
   * @param {object} data
   * @param {BrowsingContextDetails} data.contextDetails
   *     The details about the browsing context for this navigation.
   * @param {string} data.url
   *     The URL as string for the navigation.
   * @returns {NavigationInfo}
   *     The created navigation or the ongoing navigation, if applicable.
   */
  notifyNavigationStarted(data) {
    const { contextDetails, url } = data;

    const context = this.#getContextFromContextDetails(contextDetails);
    const navigableId = lazy.TabManager.getIdForBrowsingContext(context);

    let navigation = this.#navigations.get(navigableId);
    if (navigation && navigation.state === "started") {
      // Bug 1908952. As soon as we have support for the "url" field in case of beforeunload
      // prompt being open, we can remove "!navigation.url" check.
      if (!navigation.url || navigation.url === url) {
        // If we are already monitoring a navigation for this navigable and the same url,
        // for which we did not receive a navigation-stopped event, this navigation
        // is already tracked and we don't want to create another id & event.
        lazy.logger.trace(
          `[${navigableId}] Skipping already tracked navigation, navigationId: ${navigation.navigationId}`
        );
        return navigation;
      }

      lazy.logger.trace(
        `[${navigableId}] We're going to fail the navigation for url: ${navigation.url} (${navigation.navigationId}), ` +
          "since it was interrupted by a new navigation."
      );

      // If there is already a navigation in progress but with a different url,
      // it means that this navigation was interrupted by a new navigation.
      // Note: ideally we should monitor this using NS_BINDING_ABORTED,
      // but due to intermittent issues, when monitoring this in content processes,
      // we can't reliable use it.
      notifyNavigationFailed({
        contextDetails,
        errorName: "A new navigation interrupted an unfinished navigation",
        url: navigation.url,
      });
    }

    const navigationId = this.#getOrCreateNavigationId(navigableId);
    navigation = { state: "started", navigationId, url };
    this.#navigations.set(navigableId, navigation);

    lazy.logger.trace(
      lazy.truncate`[${navigableId}] Navigation started for url: ${url} (${navigationId})`
    );

    this.emit("navigation-started", { navigationId, navigableId, url });

    return navigation;
  }

  /**
   * Called when a navigation-stopped event is recorded from the
   * NavigationListener actors.
   *
   * @param {object} data
   * @param {BrowsingContextDetails} data.contextDetails
   *     The details about the browsing context for this navigation.
   * @param {string} data.url
   *     The URL as string for the navigation.
   * @returns {NavigationInfo}
   *     The stopped navigation if any, or null.
   */
  notifyNavigationStopped(data) {
    const { contextDetails, url } = data;

    const context = this.#getContextFromContextDetails(contextDetails);
    const navigableId = lazy.TabManager.getIdForBrowsingContext(context);

    const navigation = this.#navigations.get(navigableId);
    if (!navigation) {
      lazy.logger.trace(
        lazy.truncate`[${navigableId}] No navigation found to stop for url: ${url}`
      );
      return null;
    }

    if (navigation.state === "finished") {
      lazy.logger.trace(
        `[${navigableId}] Navigation already marked as finished, navigationId: ${navigation.navigationId}`
      );
      return navigation;
    }

    lazy.logger.trace(
      lazy.truncate`[${navigableId}] Navigation finished for url: ${url} (${navigation.navigationId})`
    );

    navigation.state = "finished";

    this.emit("navigation-stopped", {
      navigationId: navigation.navigationId,
      navigableId,
      url,
    });

    return navigation;
  }

  /**
   * Register a navigation id to be used for the next navigation for the
   * provided browsing context details.
   *
   * @param {object} data
   * @param {BrowsingContextDetails} data.contextDetails
   *     The details about the browsing context for this navigation.
   * @returns {string}
   *     The UUID created the upcoming navigation.
   */
  registerNavigationId(data) {
    const { contextDetails } = data;
    const context = this.#getContextFromContextDetails(contextDetails);
    const navigableId = lazy.TabManager.getIdForBrowsingContext(context);

    let navigation = this.#navigations.get(navigableId);
    if (navigation && navigation.state === "started") {
      lazy.logger.trace(
        `[${navigableId}] We're going to fail the navigation for url: ${navigation.url} (${navigation.navigationId}), ` +
          "since it was interrupted by a new navigation."
      );

      // If there is already a navigation in progress but with a different url,
      // it means that this navigation was interrupted by a new navigation.
      // Note: ideally we should monitor this using NS_BINDING_ABORTED,
      // but due to intermittent issues, when monitoring this in content processes,
      // we can't reliable use it.
      notifyNavigationFailed({
        contextDetails,
        errorName: "A new navigation interrupted an unfinished navigation",
        url: navigation.url,
      });
    }

    const navigationId = lazy.generateUUID();
    this.#navigations.set(navigableId, { state: "registered", navigationId });

    return navigationId;
  }

  #getContextFromContextDetails(contextDetails) {
    if (contextDetails.context) {
      return contextDetails.context;
    }

    return contextDetails.isTopBrowsingContext
      ? BrowsingContext.getCurrentTopByBrowserId(contextDetails.browserId)
      : BrowsingContext.get(contextDetails.browsingContextId);
  }

  #getOrCreateNavigationId(navigableId) {
    const navigation = this.#navigations.get(navigableId);
    if (navigation !== undefined && navigation.state === "registered") {
      return navigation.navigationId;
    }
    return lazy.generateUUID();
  }

  #onContextDiscarded = async (eventName, data = {}) => {
    const { browsingContext, why } = data;

    // Filter out top-level browsing contexts that are destroyed because of a
    // cross-group navigation.
    if (why === "replace") {
      return;
    }

    // TODO: Bug 1852941. We should also filter out events which are emitted
    // for DevTools frames.

    // Filter out notifications for chrome context until support gets
    // added (bug 1722679).
    if (!browsingContext.webProgress) {
      return;
    }

    const navigableId =
      lazy.TabManager.getIdForBrowsingContext(browsingContext);
    const navigation = this.#navigations.get(navigableId);

    // No need to fail navigation, if there is no navigation in progress.
    if (!navigation) {
      return;
    }

    notifyNavigationFailed({
      contextDetails: {
        context: browsingContext,
      },
      errorName: "Browsing context got discarded",
      url: navigation.url,
    });

    // If the navigable is discarded, we can safely clean up the navigation info.
    this.#navigations.delete(navigableId);
  };

  #onPromptClosed = (eventName, data) => {
    const { contentBrowser, detail } = data;
    const { accepted, promptType } = detail;

    // Send navigation failed event if beforeunload prompt was rejected.
    if (promptType === "beforeunload" && accepted === false) {
      const browsingContext = contentBrowser.browsingContext;

      notifyNavigationFailed({
        contextDetails: {
          context: browsingContext,
        },
        errorName: "Beforeunload prompt was rejected",
        // Bug 1908952. Add support for the "url" field.
      });
    }
  };

  #onPromptOpened = (eventName, data) => {
    const { contentBrowser, prompt } = data;
    const { promptType } = prompt;

    // We should start the navigation when beforeunload prompt is open.
    if (promptType === "beforeunload") {
      const browsingContext = contentBrowser.browsingContext;

      notifyNavigationStarted({
        contextDetails: {
          context: browsingContext,
        },
        // Bug 1908952. Add support for the "url" field.
      });
    }
  };
}

// Create a private NavigationRegistry singleton.
const navigationRegistry = new NavigationRegistry();

/**
 * See NavigationRegistry.notifyHashChanged.
 *
 * This entry point is only intended to be called from NavigationListenerParent,
 * to avoid setting up observers or listeners, which are unnecessary since
 * NavigationRegistry has to be a singleton.
 */
export function notifyFragmentNavigated(data) {
  return navigationRegistry.notifyFragmentNavigated(data);
}

/**
 * See NavigationRegistry.notifySameDocumentChanged.
 *
 * This entry point is only intended to be called from NavigationListenerParent,
 * to avoid setting up observers or listeners, which are unnecessary since
 * NavigationRegistry has to be a singleton.
 */
export function notifySameDocumentChanged(data) {
  return navigationRegistry.notifySameDocumentChanged(data);
}

/**
 * See NavigationRegistry.notifyNavigationFailed.
 *
 * This entry point is only intended to be called from NavigationListenerParent,
 * to avoid setting up observers or listeners, which are unnecessary since
 * NavigationRegistry has to be a singleton.
 */
export function notifyNavigationFailed(data) {
  return navigationRegistry.notifyNavigationFailed(data);
}

/**
 * See NavigationRegistry.notifyNavigationStarted.
 *
 * This entry point is only intended to be called from NavigationListenerParent,
 * to avoid setting up observers or listeners, which are unnecessary since
 * NavigationRegistry has to be a singleton.
 */
export function notifyNavigationStarted(data) {
  return navigationRegistry.notifyNavigationStarted(data);
}

/**
 * See NavigationRegistry.notifyNavigationStopped.
 *
 * This entry point is only intended to be called from NavigationListenerParent,
 * to avoid setting up observers or listeners, which are unnecessary since
 * NavigationRegistry has to be a singleton.
 */
export function notifyNavigationStopped(data) {
  return navigationRegistry.notifyNavigationStopped(data);
}

export function registerNavigationId(data) {
  return navigationRegistry.registerNavigationId(data);
}

/**
 * The NavigationManager exposes the NavigationRegistry data via a class which
 * needs to be individually instantiated by each consumer. This allow to track
 * how many consumers need navigation data at any point so that the
 * NavigationRegistry can register or unregister the underlying JSWindowActors
 * correctly.
 *
 * @fires navigation-started
 *    The NavigationManager emits "navigation-started" when a new navigation is
 *    detected, with the following object as payload:
 *      - {string} navigationId - The UUID for the navigation.
 *      - {string} navigableId - The UUID for the navigable.
 *      - {string} url - The target url for the navigation.
 * @fires navigation-stopped
 *    The NavigationManager emits "navigation-stopped" when a known navigation
 *    is stopped, with the following object as payload:
 *      - {string} navigationId - The UUID for the navigation.
 *      - {string} navigableId - The UUID for the navigable.
 *      - {string} url - The target url for the navigation.
 */
export class NavigationManager extends EventEmitter {
  #monitoring;

  constructor() {
    super();

    this.#monitoring = false;
  }

  destroy() {
    this.stopMonitoring();
  }

  getNavigationForBrowsingContext(context) {
    return navigationRegistry.getNavigationForBrowsingContext(context);
  }

  startMonitoring() {
    if (this.#monitoring) {
      return;
    }

    this.#monitoring = true;
    navigationRegistry.startMonitoring(this);
    navigationRegistry.on("fragment-navigated", this.#onNavigationEvent);
    navigationRegistry.on("navigation-failed", this.#onNavigationEvent);
    navigationRegistry.on("navigation-started", this.#onNavigationEvent);
    navigationRegistry.on("navigation-stopped", this.#onNavigationEvent);
    navigationRegistry.on("same-document-changed", this.#onNavigationEvent);
  }

  stopMonitoring() {
    if (!this.#monitoring) {
      return;
    }

    this.#monitoring = false;
    navigationRegistry.stopMonitoring(this);
    navigationRegistry.off("fragment-navigated", this.#onNavigationEvent);
    navigationRegistry.off("navigation-failed", this.#onNavigationEvent);
    navigationRegistry.off("navigation-started", this.#onNavigationEvent);
    navigationRegistry.off("navigation-stopped", this.#onNavigationEvent);
    navigationRegistry.off("same-document-changed", this.#onNavigationEvent);
  }

  #onNavigationEvent = (eventName, data) => {
    this.emit(eventName, data);
  };
}
