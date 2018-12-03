/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);
XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  UITour: "resource:///modules/UITour.jsm",
  FxAccounts: "resource://gre/modules/FxAccounts.jsm",
});
const {ASRouterActions: ra, actionTypes: at, actionCreators: ac} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});
const {CFRMessageProvider} = ChromeUtils.import("resource://activity-stream/lib/CFRMessageProvider.jsm", {});
const {OnboardingMessageProvider} = ChromeUtils.import("resource://activity-stream/lib/OnboardingMessageProvider.jsm", {});
const {SnippetsTestMessageProvider} = ChromeUtils.import("resource://activity-stream/lib/SnippetsTestMessageProvider.jsm", {});
const {RemoteSettings} = ChromeUtils.import("resource://services-settings/remote-settings.js", {});
const {CFRPageActions} = ChromeUtils.import("resource://activity-stream/lib/CFRPageActions.jsm", {});

ChromeUtils.defineModuleGetter(this, "ASRouterPreferences",
  "resource://activity-stream/lib/ASRouterPreferences.jsm");
ChromeUtils.defineModuleGetter(this, "ASRouterTargeting",
  "resource://activity-stream/lib/ASRouterTargeting.jsm");
ChromeUtils.defineModuleGetter(this, "QueryCache",
  "resource://activity-stream/lib/ASRouterTargeting.jsm");
ChromeUtils.defineModuleGetter(this, "ASRouterTriggerListeners",
  "resource://activity-stream/lib/ASRouterTriggerListeners.jsm");
ChromeUtils.import("resource:///modules/AttributionCode.jsm");

const INCOMING_MESSAGE_NAME = "ASRouter:child-to-parent";
const OUTGOING_MESSAGE_NAME = "ASRouter:parent-to-child";
const ONE_DAY_IN_MS = 24 * 60 * 60 * 1000;
// List of hosts for endpoints that serve router messages.
// Key is allowed host, value is a name for the endpoint host.
const DEFAULT_WHITELIST_HOSTS = {
  "activity-stream-icons.services.mozilla.com": "production",
  "snippets-admin.mozilla.org": "preview",
};
const SNIPPETS_ENDPOINT_WHITELIST = "browser.newtab.activity-stream.asrouter.whitelistHosts";
// Max possible impressions cap for any message
const MAX_MESSAGE_LIFETIME_CAP = 100;

const LOCAL_MESSAGE_PROVIDERS = {OnboardingMessageProvider, CFRMessageProvider, SnippetsTestMessageProvider};
const STARTPAGE_VERSION = "6";

const ADDONS_API_URL = "https://services.addons.mozilla.org/api/v3/addons/addon";

const MessageLoaderUtils = {
  STARTPAGE_VERSION,
  REMOTE_LOADER_CACHE_KEY: "RemoteLoaderCache",

  /**
   * _localLoader - Loads messages for a local provider (i.e. one that lives in mozilla central)
   *
   * @param {obj} provider An AS router provider
   * @param {Array} provider.messages An array of messages
   * @returns {Array} the array of messages
   */
  _localLoader(provider) {
    return provider.messages;
  },

  async _remoteLoaderCache(storage) {
    let allCached;
    try {
      allCached = await storage.get(MessageLoaderUtils.REMOTE_LOADER_CACHE_KEY) || {};
    } catch (e) {
      // istanbul ignore next
      Cu.reportError(e);
      // istanbul ignore next
      allCached = {};
    }
    return allCached;
  },

  /**
   * _remoteLoader - Loads messages for a remote provider
   *
   * @param {obj} provider An AS router provider
   * @param {string} provider.url An endpoint that returns an array of messages as JSON
   * @param {obj} storage A storage object with get() and set() methods for caching.
   * @returns {Promise} resolves with an array of messages, or an empty array if none could be fetched
   */
  async _remoteLoader(provider, storage) {
    let remoteMessages = [];
    if (provider.url) {
      const allCached = await MessageLoaderUtils._remoteLoaderCache(storage);
      const cached = allCached[provider.id];
      let etag;

      if (cached && cached.url === provider.url && cached.version === STARTPAGE_VERSION) {
        const {lastFetched, messages} = cached;
        if (!MessageLoaderUtils.shouldProviderUpdate({...provider, lastUpdated: lastFetched})) {
          // Cached messages haven't expired, return early.
          return messages;
        }
        etag = cached.etag;
        remoteMessages = messages;
      }

      let headers = new Headers();
      if (etag) {
        headers.set("If-None-Match", etag);
      }

      try {
        const response = await fetch(provider.url, {headers});
        if (
          // Empty response
          response.status !== 204 &&
          // Not modified
          response.status !== 304 &&
          (response.ok || response.status === 302)
        ) {
          remoteMessages = (await response.json())
            .messages
            .map(msg => ({...msg, provider_url: provider.url}));

          // Cache the results if this isn't a preview URL.
          if (provider.updateCycleInMs > 0) {
            etag = response.headers.get("ETag");
            const cacheInfo = {
              messages: remoteMessages,
              etag,
              lastFetched: Date.now(),
              version: STARTPAGE_VERSION,
            };

            storage.set(MessageLoaderUtils.REMOTE_LOADER_CACHE_KEY, {...allCached, [provider.id]: cacheInfo});
          }
        }
      } catch (e) {
        Cu.reportError(e);
      }
    }
    return remoteMessages;
  },

  /**
   * _remoteSettingsLoader - Loads messages for a RemoteSettings provider
   *
   * @param {obj} provider An AS router provider
   * @param {string} provider.bucket The name of the Remote Settings bucket
   * @returns {Promise} resolves with an array of messages, or an empty array if none could be fetched
   */
  async _remoteSettingsLoader(provider) {
    let messages = [];
    if (provider.bucket) {
      try {
        messages = await MessageLoaderUtils._getRemoteSettingsMessages(provider.bucket);
      } catch (e) {
        Cu.reportError(e);
      }
    }
    return messages;
  },

  _getRemoteSettingsMessages(bucket) {
    return RemoteSettings(bucket).get({filters: {locale: Services.locale.appLocaleAsLangTag}});
  },

  /**
   * _getMessageLoader - return the right loading function given the provider's type
   *
   * @param {obj} provider An AS Router provider
   * @returns {func} A loading function
   */
  _getMessageLoader(provider) {
    switch (provider.type) {
      case "remote":
        return this._remoteLoader;
      case "remote-settings":
        return this._remoteSettingsLoader;
      case "local":
      default:
        return this._localLoader;
    }
  },

  /**
   * shouldProviderUpdate - Given the current time, should a provider update its messages?
   *
   * @param {any} provider An AS Router provider
   * @param {int} provider.updateCycleInMs The number of milliseconds we should wait between updates
   * @param {Date} provider.lastUpdated If the provider has been updated, the time the last update occurred
   * @param {Date} currentTime The time we should check against. (defaults to Date.now())
   * @returns {bool} Should an update happen?
   */
  shouldProviderUpdate(provider, currentTime = Date.now()) {
    return (!(provider.lastUpdated >= 0) || currentTime - provider.lastUpdated > provider.updateCycleInMs);
  },

  /**
   * loadMessagesForProvider - Load messages for a provider, given the provider's type.
   *
   * @param {obj} provider An AS Router provider
   * @param {string} provider.type An AS Router provider type (defaults to "local")
   * @param {obj} storage A storage object with get() and set() methods for caching.
   * @returns {obj} Returns an object with .messages (an array of messages) and .lastUpdated (the time the messages were updated)
   */
  async loadMessagesForProvider(provider, storage) {
    const loader = this._getMessageLoader(provider);
    let messages = await loader(provider, storage);
    // istanbul ignore if
    if (!messages) {
      messages = [];
      Cu.reportError(new Error(`Tried to load messages for ${provider.id} but the result was not an Array.`));
    }
    const lastUpdated = Date.now();
    return {
      messages: messages.map(msg => ({weight: 100, ...msg, provider: provider.id}))
                        .filter(message => message.weight > 0),
      lastUpdated,
    };
  },

  async installAddonFromURL(browser, url) {
    try {
      const aUri = Services.io.newURI(url);
      const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

      // AddonManager installation source associated to the addons installed from activitystream
      // (See Bug 1496167 for a rationale).
      const amTelemetryInfo = {source: "activitystream"};
      const install = await AddonManager.getInstallForURL(aUri.spec, "application/x-xpinstall", null,
                                                          null, null, null, null, amTelemetryInfo);
      await AddonManager.installAddonFromWebpage("application/x-xpinstall", browser,
        systemPrincipal, install);
    } catch (e) {}
  },

  /**
   * cleanupCache - Removes cached data of removed providers.
   *
   * @param {Array} providers A list of activer AS Router providers
   */
  async cleanupCache(providers, storage) {
    const ids = providers.filter(p => p.type === "remote").map(p => p.id);
    const cache = await MessageLoaderUtils._remoteLoaderCache(storage);
    let dirty = false;
    for (let id in cache) {
      if (!ids.includes(id)) {
        delete cache[id];
        dirty = true;
      }
    }
    if (dirty) {
      await storage.set(MessageLoaderUtils.REMOTE_LOADER_CACHE_KEY, cache);
    }
  },
};

this.MessageLoaderUtils = MessageLoaderUtils;

/**
 * @class _ASRouter - Keeps track of all messages, UI surfaces, and
 * handles blocking, rotation, etc. Inspecting ASRouter.state will
 * tell you what the current displayed message is in all UI surfaces.
 *
 * Note: This is written as a constructor rather than just a plain object
 * so that it can be more easily unit tested.
 */
class _ASRouter {
  constructor(localProviders = LOCAL_MESSAGE_PROVIDERS) {
    this.initialized = false;
    this.messageChannel = null;
    this.dispatchToAS = null;
    this._storage = null;
    this._resetInitialization();
    this._state = {
      lastMessageId: null,
      providers: [],
      messageBlockList: [],
      providerBlockList: [],
      messageImpressions: {},
      providerImpressions: {},
      messages: [],
    };
    this._triggerHandler = this._triggerHandler.bind(this);
    this._localProviders = localProviders;
    this.onMessage = this.onMessage.bind(this);
    this._handleTargetingError = this._handleTargetingError.bind(this);
    this.onPrefChange = this.onPrefChange.bind(this);
  }

  // Update message providers and fetch new messages on pref change
  async onPrefChange() {
    this._updateMessageProviders();
    await this.loadMessagesFromAllProviders();
    this.dispatchToAS(ac.BroadcastToContent({type: at.AS_ROUTER_PREF_CHANGED, data: ASRouterPreferences.specialConditions}));
  }

  // Replace all frequency time period aliases with their millisecond values
  // This allows us to avoid accounting for special cases later on
  normalizeItemFrequency({frequency}) {
    if (frequency && frequency.custom) {
      for (const setting of frequency.custom) {
        if (setting.period === "daily") {
          setting.period = ONE_DAY_IN_MS;
        }
      }
    }
  }

  // Fetch and decode the message provider pref JSON, and update the message providers
  _updateMessageProviders() {
    const previousProviders =  this.state.providers;
    const providers = [
      // If we have added a `preview` provider, hold onto it
      ...previousProviders.filter(p => p.id === "preview"),
      // The provider should be enabled and not have a user preference set to false
      ...ASRouterPreferences.providers.filter(p => (
        p.enabled &&
        ASRouterPreferences.getUserPreference(p.id) !== false)
      ),
    ].map(_provider => {
      // make a copy so we don't modify the source of the pref
      const provider = {..._provider};

      if (provider.type === "local" && !provider.messages) {
        // Get the messages from the local message provider
        const localProvider = this._localProviders[provider.localProvider];
        provider.messages = localProvider ? localProvider.getMessages() : [];
      }
      if (provider.type === "remote" && provider.url) {
        provider.url = provider.url.replace(/%STARTPAGE_VERSION%/g, STARTPAGE_VERSION);
        provider.url = Services.urlFormatter.formatURL(provider.url);
      }
      this.normalizeItemFrequency(provider);
      // Reset provider update timestamp to force message refresh
      provider.lastUpdated = undefined;
      return provider;
    });

    const providerIDs = providers.map(p => p.id);

    // Clear old messages for providers that are no longer enabled
    for (const prevProvider of previousProviders) {
      if (!providerIDs.includes(prevProvider.id)) {
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_PROVIDER", data: {id: prevProvider.id}});
      }
    }

    this.setState(prevState => ({
      providers,
      // Clear any messages from removed providers
      messages: [...prevState.messages.filter(message => providerIDs.includes(message.provider))],
    }));
  }

  get state() {
    return this._state;
  }

  set state(value) {
    throw new Error("Do not modify this.state directy. Instead, call this.setState(newState)");
  }

  /**
   * _resetInitialization - adds the following to the instance:
   *  .initialized {bool}            Has AS Router been initialized?
   *  .waitForInitialized {Promise}  A promise that resolves when initializion is complete
   *  ._finishInitializing {func}    A function that, when called, resolves the .waitForInitialized
   *                                 promise and sets .initialized to true.
   * @memberof _ASRouter
   */
  _resetInitialization() {
    this.initialized = false;
    this.waitForInitialized = new Promise(resolve => {
      this._finishInitializing = () => {
        this.initialized = true;
        resolve();
      };
    });
  }

  /**
   * loadMessagesFromAllProviders - Loads messages from all providers if they require updates.
   *                                Checks the .lastUpdated field on each provider to see if updates are needed
   * @memberof _ASRouter
   */
  async loadMessagesFromAllProviders() {
    const needsUpdate = this.state.providers.filter(provider => MessageLoaderUtils.shouldProviderUpdate(provider));
    // Don't do extra work if we don't need any updates
    if (needsUpdate.length) {
      let newState = {messages: [], providers: []};
      for (const provider of this.state.providers) {
        if (needsUpdate.includes(provider)) {
          const {messages, lastUpdated} = await MessageLoaderUtils.loadMessagesForProvider(provider, this._storage);
          newState.providers.push({...provider, lastUpdated});
          newState.messages = [...newState.messages, ...messages];
        } else {
          // Skip updating this provider's messages if no update is required
          let messages = this.state.messages.filter(msg => msg.provider === provider.id);
          newState.providers.push(provider);
          newState.messages = [...newState.messages, ...messages];
        }
      }

      for (const message of newState.messages) {
        this.normalizeItemFrequency(message);
      }

      // Some messages have triggers that require us to initalise trigger listeners
      const unseenListeners = new Set(ASRouterTriggerListeners.keys());
      for (const {trigger} of newState.messages) {
        if (trigger && ASRouterTriggerListeners.has(trigger.id)) {
          ASRouterTriggerListeners.get(trigger.id).init(this._triggerHandler, trigger.params);
          unseenListeners.delete(trigger.id);
        }
      }
      // We don't need these listeners, but they may have previously been
      // initialised, so uninitialise them
      for (const triggerID of unseenListeners) {
        ASRouterTriggerListeners.get(triggerID).uninit();
      }

      // We don't want to cache preview endpoints, remove them after messages are fetched
      await this.setState(this._removePreviewEndpoint(newState));
      await this.cleanupImpressions();
    }
  }

  /**
   * init - Initializes the MessageRouter.
   * It is ready when it has been connected to a RemotePageManager instance.
   *
   * @param {RemotePageManager} channel a RemotePageManager instance
   * @param {obj} storage an AS storage instance
   * @param {func} dispatchToAS dispatch an action the main AS Store
   * @memberof _ASRouter
   */
  async init(channel, storage, dispatchToAS) {
    this.messageChannel = channel;
    this.messageChannel.addMessageListener(INCOMING_MESSAGE_NAME, this.onMessage);
    this._storage = storage;
    this.WHITELIST_HOSTS = this._loadSnippetsWhitelistHosts();
    this.dispatchToAS = dispatchToAS;
    this.dispatch = this.dispatch.bind(this);

    ASRouterPreferences.init();
    ASRouterPreferences.addListener(this.onPrefChange);

    const messageBlockList = await this._storage.get("messageBlockList") || [];
    const providerBlockList = await this._storage.get("providerBlockList") || [];
    const messageImpressions = await this._storage.get("messageImpressions") || {};
    const providerImpressions = await this._storage.get("providerImpressions") || {};
    const previousSessionEnd = await this._storage.get("previousSessionEnd") || 0;
    await this.setState({messageBlockList, providerBlockList, messageImpressions, providerImpressions, previousSessionEnd});
    this._updateMessageProviders();
    await this.loadMessagesFromAllProviders();
    await MessageLoaderUtils.cleanupCache(this.state.providers, storage);

    // set necessary state in the rest of AS
    this.dispatchToAS(ac.BroadcastToContent({type: at.AS_ROUTER_INITIALIZED, data: ASRouterPreferences.specialConditions}));

    // sets .initialized to true and resolves .waitForInitialized promise
    this._finishInitializing();
  }

  uninit() {
    this._storage.set("previousSessionEnd", Date.now());

    this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_ALL"});
    this.messageChannel.removeMessageListener(INCOMING_MESSAGE_NAME, this.onMessage);
    this.messageChannel = null;
    this.dispatchToAS = null;

    ASRouterPreferences.removeListener(this.onPrefChange);
    ASRouterPreferences.uninit();

    // Uninitialise all trigger listeners
    for (const listener of ASRouterTriggerListeners.values()) {
      listener.uninit();
    }
    // If we added any CFR recommendations, they need to be removed
    CFRPageActions.clearRecommendations();
    this._resetInitialization();
  }

  setState(callbackOrObj) {
    const newState = (typeof callbackOrObj === "function") ? callbackOrObj(this.state) : callbackOrObj;
    this._state = {...this.state, ...newState};
    return new Promise(resolve => {
      this._onStateChanged(this.state);
      resolve();
    });
  }

  getMessageById(id) {
    return this.state.messages.find(message => message.id === id);
  }

  _onStateChanged(state) {
    if (ASRouterPreferences.devtoolsEnabled) {
      this._updateAdminState();
    }
  }

  /**
   * Used by ASRouter Admin returns all ASRouterTargeting.Environment
   * and ASRouter._getMessagesContext parameters and values
   */
  async getTargetingParameters(environment, localContext) {
    const targetingParameters = {};
    for (const param of Object.keys(environment)) {
      targetingParameters[param] = await environment[param];
    }
    for (const param of Object.keys(localContext)) {
      targetingParameters[param] = await localContext[param];
    }

    return targetingParameters;
  }

  async _updateAdminState(target) {
    const channel = target || this.messageChannel;
    channel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
      type: "ADMIN_SET_STATE",
      data: {
        ...this.state,
        providerPrefs: ASRouterPreferences.providers,
        userPrefs: ASRouterPreferences.getAllUserPreferences(),
        targetingParameters: await this.getTargetingParameters(ASRouterTargeting.Environment, this._getMessagesContext()),
      },
    });
  }

  _handleTargetingError(type, error, message) {
    Cu.reportError(error);
    if (this.dispatchToAS) {
      this.dispatchToAS(ac.ASRouterUserEvent({
        message_id: message.id,
        action: "asrouter_undesired_event",
        event: "TARGETING_EXPRESSION_ERROR",
        value: type,
      }));
    }
  }

  // Return an object containing targeting parameters used to select messages
  _getMessagesContext() {
    const {previousSessionEnd} = this.state;
    return {
      get previousSessionEnd() {
        return previousSessionEnd;
      },
    };
  }

  _findMessage(candidateMessages, trigger) {
    const messages = candidateMessages.filter(m => this.isBelowFrequencyCaps(m));
    const context = this._getMessagesContext();

     // Find a message that matches the targeting context as well as the trigger context (if one is provided)
     // If no trigger is provided, we should find a message WITHOUT a trigger property defined.
    return ASRouterTargeting.findMatchingMessage({messages, trigger, context, onError: this._handleTargetingError});
  }

  async evaluateExpression(target, {expression, context}) {
    const channel = target || this.messageChannel;
    let evaluationStatus;
    try {
      evaluationStatus = {result: await ASRouterTargeting.isMatch(expression, context), success: true};
    } catch (e) {
      evaluationStatus = {result: e.message, success: false};
    }

    channel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "ADMIN_SET_STATE", data: {...this.state, evaluationStatus}});
  }

  _orderBundle(bundle) {
    return bundle.sort((a, b) => a.order - b.order);
  }

  // Work out if a message can be shown based on its and its provider's frequency caps.
  isBelowFrequencyCaps(message) {
    const {providers, messageImpressions, providerImpressions} = this.state;

    const provider = providers.find(p => p.id === message.provider);
    const impressionsForMessage = messageImpressions[message.id];
    const impressionsForProvider = providerImpressions[message.provider];

    return (this._isBelowItemFrequencyCap(message, impressionsForMessage, MAX_MESSAGE_LIFETIME_CAP) &&
      this._isBelowItemFrequencyCap(provider, impressionsForProvider));
  }

  // Helper for isBelowFrecencyCaps - work out if the frequency cap for the given
  //                                  item has been exceeded or not
  _isBelowItemFrequencyCap(item, impressions, maxLifetimeCap = Infinity) {
    if (item && item.frequency && impressions && impressions.length) {
      if (
        item.frequency.lifetime &&
        impressions.length >= Math.min(item.frequency.lifetime, maxLifetimeCap)
      ) {
        return false;
      }
      if (item.frequency.custom) {
        const now = Date.now();
        for (const setting of item.frequency.custom) {
          let {period} = setting;
          const impressionsInPeriod = impressions.filter(t => (now - t) < period);
          if (impressionsInPeriod.length >= setting.cap) {
            return false;
          }
        }
      }
    }
    return true;
  }

  async _getBundledMessages(originalMessage, target, trigger, force = false) {
    let result = [{content: originalMessage.content, id: originalMessage.id, order: originalMessage.order || 0}];

    // First, find all messages of same template. These are potential matching targeting candidates
    let bundledMessagesOfSameTemplate = this._getUnblockedMessages()
                                          .filter(msg => msg.bundled && msg.template === originalMessage.template && msg.id !== originalMessage.id);

    if (force) {
      // Forcefully show the messages without targeting matching - this is for about:newtab#asrouter to show the messages
      for (const message of bundledMessagesOfSameTemplate) {
        result.push({content: message.content, id: message.id});
        // Stop once we have enough messages to fill a bundle
        if (result.length === originalMessage.bundled) {
          break;
        }
      }
    } else {
      while (bundledMessagesOfSameTemplate.length) {
        // Find a message that matches the targeting context - or break if there are no matching messages
        const message = await this._findMessage(bundledMessagesOfSameTemplate, trigger);
        if (!message) {
          /* istanbul ignore next */ // Code coverage in mochitests
          break;
        }
        // Only copy the content of the message (that's what the UI cares about)
        // Also delete the message we picked so we don't pick it again
        result.push({content: message.content, id: message.id, order: message.order || 0});
        bundledMessagesOfSameTemplate.splice(bundledMessagesOfSameTemplate.findIndex(msg => msg.id === message.id), 1);
        // Stop once we have enough messages to fill a bundle
        if (result.length === originalMessage.bundled) {
          break;
        }
      }
    }

    // If we did not find enough messages to fill the bundle, do not send the bundle down
    if (result.length < originalMessage.bundled) {
      return null;
    }

    // The bundle may have some extra attributes, like a header, or a dismiss button, so attempt to get those strings now
    // This is a temporary solution until we can use Fluent strings in the content process, in which case the content can
    // handle finding these strings on its own. See bug 1488973
    const extraTemplateStrings = await this._extraTemplateStrings(originalMessage);

    return {bundle: this._orderBundle(result), ...(extraTemplateStrings && {extraTemplateStrings}), provider: originalMessage.provider, template: originalMessage.template};
  }

  async _extraTemplateStrings(originalMessage) {
    let extraTemplateStrings;
    let localProvider = this._findProvider(originalMessage.provider);
    if (localProvider && localProvider.getExtraAttributes) {
      extraTemplateStrings = await localProvider.getExtraAttributes();
    }

    return extraTemplateStrings;
  }

  _findProvider(providerID) {
    return this._localProviders[this.state.providers.find(i => i.id === providerID).localProvider];
  }

  _getUnblockedMessages() {
    let {state} = this;
    return state.messages.filter(item =>
      !state.messageBlockList.includes(item.id) &&
      (!item.campaign || !state.messageBlockList.includes(item.campaign)) &&
      !state.providerBlockList.includes(item.provider)
    );
  }

  async _sendMessageToTarget(message, target, trigger, force = false) {
    // No message is available, so send CLEAR_ALL.
    if (!message) {
      try {
        target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_ALL"});
      } catch (e) {}

    // For bundled messages, look for the rest of the bundle or else send CLEAR_ALL
    } else if (message.bundled) {
      const bundledMessages = await this._getBundledMessages(message, target, trigger, force);
      const action = bundledMessages ? {type: "SET_BUNDLED_MESSAGES", data: bundledMessages} : {type: "CLEAR_ALL"};
      target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, action);

    // CFR doorhanger
    } else if (message.template === "cfr_doorhanger") {
      if (force) {
        CFRPageActions.forceRecommendation(target, message, this.dispatch);
      } else {
        CFRPageActions.addRecommendation(target, trigger.param, message, this.dispatch);
      }

    // New tab single messages
    } else {
      target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "SET_MESSAGE", data: message});
    }
  }

  async addImpression(message) {
    const provider = this.state.providers.find(p => p.id === message.provider);
    // We only need to store impressions for messages that have frequency, or
    // that have providers that have frequency
    if (message.frequency || (provider && provider.frequency)) {
      const time = Date.now();
      await this.setState(state => {
        const messageImpressions = this._addImpressionForItem(state, message, "messageImpressions", time);
        const providerImpressions = this._addImpressionForItem(state, provider, "providerImpressions", time);
        return {messageImpressions, providerImpressions};
      });
    }
  }

  // Helper for addImpression - calculate the updated impressions object for the given
  //                            item, then store it and return it
  _addImpressionForItem(state, item, impressionsString, time) {
    // The destructuring here is to avoid mutating existing objects in state as in redux
    // (see https://redux.js.org/recipes/structuring-reducers/prerequisite-concepts#immutable-data-management)
    const impressions = {...state[impressionsString]};
    if (item.frequency) {
      impressions[item.id] = impressions[item.id] ? [...impressions[item.id]] : [];
      impressions[item.id].push(time);
      this._storage.set(impressionsString, impressions);
    }
    return impressions;
  }

  /**
   * getLongestPeriod
   *
   * @param {obj} item Either an ASRouter message or an ASRouter provider
   * @returns {int|null} if the item has custom frequency caps, the longest period found in the list of caps.
                         if the item has no custom frequency caps, null
   * @memberof _ASRouter
   */
  getLongestPeriod(item) {
    if (!item.frequency || !item.frequency.custom) {
      return null;
    }
    return item.frequency.custom.sort((a, b) => b.period - a.period)[0].period;
  }

  /**
   * cleanupImpressions - this function cleans up obsolete impressions whenever
   * messages are refreshed or fetched. It will likely need to be more sophisticated in the future,
   * but the current behaviour for when both message impressions and provider impressions are
   * cleared is as follows (where `item` is either `message` or `provider`):
   *
   * 1. If the item id for a list of item impressions no longer exists in the ASRouter state, it
   *    will be cleared.
   * 2. If the item has time-bound frequency caps but no lifetime cap, any item impressions older
   *    than the longest time period will be cleared.
   */
  async cleanupImpressions() {
    await this.setState(state => {
      const messageImpressions = this._cleanupImpressionsForItems(state, state.messages, "messageImpressions");
      const providerImpressions = this._cleanupImpressionsForItems(state, state.providers, "providerImpressions");
      return {messageImpressions, providerImpressions};
    });
  }

  // Helper for cleanupImpressions - calculate the updated impressions object for
  //                                 the given items, then store it and return it
  _cleanupImpressionsForItems(state, items, impressionsString) {
    const impressions = {...state[impressionsString]};
    let needsUpdate = false;
    Object.keys(impressions).forEach(id => {
      const [item] = items.filter(x => x.id === id);
      // Don't keep impressions for items that no longer exist
      if (!item || !item.frequency || !Array.isArray(impressions[id])) {
        delete impressions[id];
        needsUpdate = true;
        return;
      }
      if (!impressions[id].length) {
        return;
      }
      // If we don't want to store impressions older than the longest period
      if (item.frequency.custom && !item.frequency.lifetime) {
        const now = Date.now();
        impressions[id] = impressions[id].filter(t => (now - t) < this.getLongestPeriod(item));
        needsUpdate = true;
      }
    });
    if (needsUpdate) {
      this._storage.set(impressionsString, impressions);
    }
    return impressions;
  }

  async _fetchAddonInfo() {
    let data = {};
    const {content} = await AttributionCode.getAttrDataAsync();
    if (!content) {
      return data;
    }
    try {
      const response = await fetch(`${ADDONS_API_URL}/${content}`);
      if (response.status !== 204 && response.ok) {
        const json = await response.json();
        data.url = json.current_version.files[0].url;
        data.iconURL = json.icon_url;
      }
    } catch (e) {
      Cu.reportError("Failed to get the latest add-on version for Return to AMO");
    }
    return data;
  }

  async sendNextMessage(target, trigger) {
    const msgs = this._getUnblockedMessages();
    let message = null;
    const previewMsgs = this.state.messages.filter(item => item.provider === "preview");
    // Always send preview messages when available
    if (previewMsgs.length) {
      [message] = previewMsgs;
    } else {
      message = await this._findMessage(msgs, trigger);
    }

    // We need some addon info if we are showing return to amo overlay, so fetch
    // that, and update the message accordingly
    if (message && message.template === "return_to_amo_overlay") {
      const {url, iconURL} = await this._fetchAddonInfo();

      // If we failed to get this info, we do not want to show this message
      if (!url || !iconURL) {
        return;
      }
      message.content.addon_icon = iconURL;
      message.content.primary_button.action.data.url = url;
    }

    if (previewMsgs.length) {
      // We don't want to cache preview messages, remove them after we selected the message to show
      await this.setState(state => ({
        lastMessageId: message.id,
        messages: state.messages.filter(m => m.id !== message.id),
      }));
    } else {
      await this.setState({lastMessageId: message ? message.id : null});
    }
    await this._sendMessageToTarget(message, target, trigger);
  }

  async setMessageById(id, target, force = true, action = {}) {
    await this.setState({lastMessageId: id});
    const newMessage = this.getMessageById(id);

    await this._sendMessageToTarget(newMessage, target, action.data, force);
  }

  async blockMessageById(idOrIds) {
    const idsToBlock = Array.isArray(idOrIds) ? idOrIds : [idOrIds];

    await this.setState(state => {
      const messageBlockList = [...state.messageBlockList];
      const messageImpressions = {...state.messageImpressions};

      idsToBlock.forEach(id => {
        const message = state.messages.find(m => m.id === id);
        const idToBlock = (message && message.campaign) ? message.campaign : id;
        if (!messageBlockList.includes(idToBlock)) {
          messageBlockList.push(idToBlock);
        }

        // When a message is blocked, its impressions should be cleared as well
        delete messageImpressions[id];
      });

      this._storage.set("messageBlockList", messageBlockList);
      return {messageBlockList, messageImpressions};
    });
  }

  async blockProviderById(idOrIds) {
    const idsToBlock = Array.isArray(idOrIds) ? idOrIds : [idOrIds];

    await this.setState(state => {
      const providerBlockList = [...state.providerBlockList, ...idsToBlock];
      // When a provider is blocked, its impressions should be cleared as well
      const providerImpressions = {...state.providerImpressions};
      idsToBlock.forEach(id => delete providerImpressions[id]);
      this._storage.set("providerBlockList", providerBlockList);
      return {providerBlockList, providerImpressions};
    });
  }

  _validPreviewEndpoint(url) {
    try {
      const endpoint = new URL(url);
      if (!this.WHITELIST_HOSTS[endpoint.host]) {
        Cu.reportError(`The preview URL host ${endpoint.host} is not in the whitelist.`);
      }
      if (endpoint.protocol !== "https:") {
        Cu.reportError("The URL protocol is not https.");
      }
      return (endpoint.protocol === "https:" && this.WHITELIST_HOSTS[endpoint.host]);
    } catch (e) {
      return false;
    }
  }

  _loadSnippetsWhitelistHosts() {
    let additionalHosts = [];
    const whitelistPrefValue = Services.prefs.getStringPref(SNIPPETS_ENDPOINT_WHITELIST, "");
    try {
      additionalHosts = JSON.parse(whitelistPrefValue);
    } catch (e) {
      if (whitelistPrefValue) {
        Cu.reportError(`Pref ${SNIPPETS_ENDPOINT_WHITELIST} value is not valid JSON`);
      }
    }

    if (!additionalHosts.length) {
      return DEFAULT_WHITELIST_HOSTS;
    }

    // If there are additional hosts we want to whitelist, add them as
    // `preview` so that the updateCycle is 0
    return additionalHosts.reduce((whitelist_hosts, host) => {
      whitelist_hosts[host] = "preview";
      Services.console.logStringMessage(`Adding ${host} to whitelist hosts.`);
      return whitelist_hosts;
    }, {...DEFAULT_WHITELIST_HOSTS});
  }

  // To be passed to ASRouterTriggerListeners
  async _triggerHandler(target, trigger) {
    await this.onMessage({target, data: {type: "TRIGGER", data: {trigger}}});
  }

  _removePreviewEndpoint(state) {
    state.providers = state.providers.filter(p => p.id !== "preview");
    return state;
  }

  async _addPreviewEndpoint(url, portID) {
    // When you view a preview snippet we want to hide all real content
    const providers = [...this.state.providers];
    if (this._validPreviewEndpoint(url) && !providers.find(p => p.url === url)) {
      this.dispatchToAS(ac.OnlyToOneContent({type: at.SNIPPETS_PREVIEW_MODE}, portID));
      providers.push({id: "preview", type: "remote", url, updateCycleInMs: 0});
      await this.setState({providers});
    }
  }

  /**
   * forceAttribution - this function should only be called from within about:newtab#asrouter.
   * It forces the browser attribution to be set to something specified in asrouter admin
   * tools, and reloads the providers in order to get messages that are dependant on this
   * attribution data (see Return to AMO flow in bug 1475354 for example). Note - only works with OSX
   * @param {data} Object an object containing the attribtion data that came from asrouter admin page
   */
  async forceAttribution(data) {
    // Extract the parameters from data that will make up the referrer url
    const {source, campaign, content} = data;
    let appPath = Services.dirsvc.get("GreD", Ci.nsIFile).parent.parent.path;
    let attributionSvc = Cc["@mozilla.org/mac-attribution;1"]
                            .getService(Ci.nsIMacAttributionService);

    let referrer = `https://www.mozilla.org/anything/?utm_campaign=${campaign}&utm_source=${source}&utm_content=${encodeURIComponent(content)}`;

    // This sets the Attribution to be the referrer
    attributionSvc.setReferrerUrl(appPath, referrer, true);
    let env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
    env.set("XPCSHELL_TEST_PROFILE_DIR", "testing");

    // Clear and refresh Attribution, and then fetch the messages again to update
    AttributionCode._clearCache();
    AttributionCode.getAttrDataAsync();
    this._updateMessageProviders();
    await this.loadMessagesFromAllProviders();
  }

  async handleUserAction({data: action, target}) {
    switch (action.type) {
      case ra.OPEN_PRIVATE_BROWSER_WINDOW:
        // Forcefully open about:privatebrowsing
        target.browser.ownerGlobal.OpenBrowserWindow({private: true});
        break;
      case ra.OPEN_URL:
        target.browser.ownerGlobal.openLinkIn(action.data.args, action.data.where || "current", {
          private: false,
          triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({}),
        });
        break;
      case ra.OPEN_ABOUT_PAGE:
        target.browser.ownerGlobal.openTrustedLinkIn(`about:${action.data.args}`, "tab");
        break;
      case ra.OPEN_PREFERENCES_PAGE:
        target.browser.ownerGlobal.openPreferences(action.data.category, {origin: action.data.origin});
        break;
      case ra.OPEN_APPLICATIONS_MENU:
        UITour.showMenu(target.browser.ownerGlobal, action.data.args);
        break;
      case ra.INSTALL_ADDON_FROM_URL:
        await MessageLoaderUtils.installAddonFromURL(target.browser, action.data.url);
        break;
      case ra.SHOW_FIREFOX_ACCOUNTS:
        const url = await FxAccounts.config.promiseSignUpURI("snippets");
        // We want to replace the current tab.
        target.browser.ownerGlobal.openLinkIn(url, "current", {
          private: false,
          triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({}),
        });
        break;
    }
  }

  dispatch(action, target) {
    this.onMessage({data: action, target});
  }

  async onMessage({data: action, target}) {
    switch (action.type) {
      case "USER_ACTION":
        if (action.data.type in ra) {
          await this.handleUserAction({data: action.data, target});
        }
        break;
      case "SNIPPETS_REQUEST":
      case "TRIGGER":
        // Wait for our initial message loading to be done before responding to any UI requests
        await this.waitForInitialized;
        if (action.data && action.data.endpoint) {
          await this._addPreviewEndpoint(action.data.endpoint.url, target.portID);
        }
        // Check if any updates are needed first
        await this.loadMessagesFromAllProviders();
        await this.sendNextMessage(target, (action.data && action.data.trigger) || {});
        break;
      case "BLOCK_MESSAGE_BY_ID":
        await this.blockMessageById(action.data.id);
        // Block the message but don't dismiss it in case the action taken has
        // another state that needs to be visible
        if (action.data.preventDismiss) {
          break;
        }
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_MESSAGE", data: {id: action.data.id}});
        break;
      case "DISMISS_MESSAGE_BY_ID":
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_MESSAGE", data: {id: action.data.id}});
        break;
      case "BLOCK_PROVIDER_BY_ID":
        await this.blockProviderById(action.data.id);
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_PROVIDER", data: {id: action.data.id}});
        break;
      case "DISMISS_BUNDLE":
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_BUNDLE"});
        break;
      case "BLOCK_BUNDLE":
        await this.blockMessageById(action.data.bundle.map(b => b.id));
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_BUNDLE"});
        break;
      case "UNBLOCK_MESSAGE_BY_ID":
        await this.setState(state => {
          const messageBlockList = [...state.messageBlockList];
          const message = state.messages.find(m => m.id === action.data.id);
          const idToUnblock = (message && message.campaign) ? message.campaign : action.data.id;
          messageBlockList.splice(messageBlockList.indexOf(idToUnblock), 1);
          this._storage.set("messageBlockList", messageBlockList);
          return {messageBlockList};
        });
        break;
      case "UNBLOCK_PROVIDER_BY_ID":
        await this.setState(state => {
          const providerBlockList = [...state.providerBlockList];
          providerBlockList.splice(providerBlockList.indexOf(action.data.id), 1);
          this._storage.set("providerBlockList", providerBlockList);
          return {providerBlockList};
        });
        break;
      case "UNBLOCK_BUNDLE":
        await this.setState(state => {
          const messageBlockList = [...state.messageBlockList];
          for (let message of action.data.bundle) {
            messageBlockList.splice(messageBlockList.indexOf(message.id), 1);
          }
          this._storage.set("messageBlockList", messageBlockList);
          return {messageBlockList};
        });
        break;
      case "OVERRIDE_MESSAGE":
        await this.setMessageById(action.data.id, target, true, action);
        break;
      case "ADMIN_CONNECT_STATE":
        if (action.data && action.data.endpoint) {
          this._addPreviewEndpoint(action.data.endpoint.url, target.portID);
          await this.loadMessagesFromAllProviders();
        } else {
          await this._updateAdminState(target);
        }
        break;
      case "IMPRESSION":
        await this.addImpression(action.data);
        break;
      case "DOORHANGER_TELEMETRY":
        if (this.dispatchToAS) {
          this.dispatchToAS(ac.ASRouterUserEvent(action.data));
        }
        break;
      case "EXPIRE_QUERY_CACHE":
        QueryCache.expireAll();
        break;
      case "ENABLE_PROVIDER":
        ASRouterPreferences.enableOrDisableProvider(action.data, true);
        break;
      case "DISABLE_PROVIDER":
        ASRouterPreferences.enableOrDisableProvider(action.data, false);
        break;
      case "RESET_PROVIDER_PREF":
        ASRouterPreferences.resetProviderPref();
        break;
      case "SET_PROVIDER_USER_PREF":
        ASRouterPreferences.setUserPreference(action.data.id, action.data.value);
        break;
      case "EVALUATE_JEXL_EXPRESSION":
        this.evaluateExpression(target, action.data);
        break;
      case "FORCE_ATTRIBUTION":
        this.forceAttribution(action.data);
    }
  }
}
this._ASRouter = _ASRouter;

/**
 * ASRouter - singleton instance of _ASRouter that controls all messages
 * in the new tab page.
 */
this.ASRouter = new _ASRouter();

const EXPORTED_SYMBOLS = ["_ASRouter", "ASRouter", "MessageLoaderUtils"];
