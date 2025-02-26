/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { RootBiDiModule } from "chrome://remote/content/webdriver-bidi/modules/RootBiDiModule.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  ContextDescriptorType:
    "chrome://remote/content/shared/messagehandler/MessageHandler.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  generateUUID: "chrome://remote/content/shared/UUID.sys.mjs",
  getWebDriverSessionById:
    "chrome://remote/content/shared/webdriver/Session.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
  RootMessageHandler:
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  UserContextManager:
    "chrome://remote/content/shared/UserContextManager.sys.mjs",
});
class SessionModule extends RootBiDiModule {
  #knownSubscriptionIds;
  #subscriptions;

  /**
   * An object that holds information about the subscription,
   * if the <var>topLevelTraversableIds</var> and the <var>userContextIds</var>
   * are both empty, the subscription is considered global.
   *
   * @typedef Subscription
   *
   * @property {Set} eventNames
   *     A set of event names related to this subscription.
   * @property {string} subscriptionId
   *     A unique subscription identifier.
   * @property {Set} topLevelTraversableIds
   *     A set of top level traversable ids related to this subscription.
   * @property {Set} userContextIds
   *     A set of user context ids related to this subscription.
   */

  constructor(messageHandler) {
    super(messageHandler);

    // Set of subscription ids.
    this.#knownSubscriptionIds = new Set();
    // List of subscription objects type Subscription.
    this.#subscriptions = [];
  }

  destroy() {
    this.#knownSubscriptionIds = null;
    this.#subscriptions = null;
  }

  /**
   * Commands
   */

  /**
   * End the current session.
   *
   * Session clean up will happen later in WebDriverBiDiConnection class.
   */
  async end() {
    const session = lazy.getWebDriverSessionById(this.messageHandler.sessionId);

    if (session.http) {
      throw new lazy.error.UnsupportedOperationError(
        "Ending a session started with WebDriver classic is not supported." +
          ' Use the WebDriver classic "Delete Session" command instead.'
      );
    }
  }

  /**
   * An object that holds a unique subscription identifier.
   *
   * @typedef SubscribeResult
   *
   * @property {string} subscription
   *     A unique subscription identifier.
   */

  /**
   * Enable certain events either globally, or for a list of browsing contexts.
   *
   * @param {object=} params
   * @param {Array<string>} params.events
   *     List of events to subscribe to.
   * @param {Array<string>=} params.contexts
   *     Optional list of top-level browsing context ids
   *     to subscribe the events for.
   * @param {Array<string>=} params.userContexts
   *     Optional list of user context ids
   *     to subscribe the events for.
   *
   * @returns {SubscribeResult}
   *     A unique subscription identifier.
   * @throws {InvalidArgumentError}
   *     If <var>events</var> or <var>contexts</var> are not valid types.
   */
  async subscribe(params = {}) {
    const { events, contexts: contextIds = null, userContexts = null } = params;

    // Check input types until we run schema validation.
    this.#assertNonEmptyArrayWithStrings(events, "events");

    if (contextIds !== null) {
      this.#assertNonEmptyArrayWithStrings(contextIds, "contexts");
    }

    if (userContexts !== null) {
      this.#assertNonEmptyArrayWithStrings(userContexts, "userContexts");
    }

    const eventNames = new Set();
    events.forEach(name => {
      this.#obtainEvents(name).forEach(event => eventNames.add(event));
    });

    const inputUserContextIds = new Set(userContexts);
    const inputContextIds = new Set(contextIds);

    if (inputUserContextIds.size > 0 && inputContextIds.size > 0) {
      throw new lazy.error.InvalidArgumentError(
        `Providing both "userContexts" and "contexts" arguments is not supported`
      );
    }

    let subscriptionNavigables = new Set();
    const topLevelTraversableContextIds = new Set();
    const userContextIds = new Set();

    if (inputContextIds.size !== 0) {
      const navigables = this.#getValidNavigablesByIds(inputContextIds);
      subscriptionNavigables = this.#getTopLevelTraversables(navigables);

      for (const navigable of subscriptionNavigables) {
        topLevelTraversableContextIds.add(
          lazy.TabManager.getIdForBrowsingContext(navigable)
        );
      }
    } else if (inputUserContextIds.size !== 0) {
      for (const userContextId of inputUserContextIds) {
        const internalId =
          lazy.UserContextManager.getInternalIdById(userContextId);

        if (internalId === null) {
          throw new lazy.error.NoSuchUserContextError(
            `User context with id: ${userContextId} doesn't exist`
          );
        }

        lazy.UserContextManager.getTabsForUserContext(internalId).forEach(
          item => subscriptionNavigables.add(item)
        );

        userContextIds.add(internalId);
      }
    } else {
      for (const tab of lazy.TabManager.tabs) {
        subscriptionNavigables.add(tab);
      }
    }

    const subscription = {
      eventNames,
      subscriptionId: lazy.generateUUID(),
      topLevelTraversableIds: topLevelTraversableContextIds,
      userContextIds,
    };

    const subscribeStepEvents = new Map();

    for (const eventName of eventNames) {
      const existingNavigables =
        this.#getEnabledTopLevelTraversables(eventName);

      subscribeStepEvents.set(
        eventName,
        subscriptionNavigables.difference(existingNavigables)
      );
    }

    this.#subscriptions.push(subscription);
    this.#knownSubscriptionIds.add(subscription.subscriptionId);

    // TODO: Bug 1801284. Add subscribe priority sorting of subscribeStepEvents (step 4 to 6, and 8).

    const includeGlobal = this.#isSubscriptionGlobal(subscription);

    const listeners = this.#getListenersToSubscribe(
      eventNames,
      includeGlobal,
      subscribeStepEvents,
      userContextIds
    );

    // Subscribe to the relevant engine-internal events.
    await this.messageHandler.eventsDispatcher.update(listeners);

    return { subscription: subscription.subscriptionId };
  }

  /**
   * Disable certain events either globally, for a list of browsing contexts
   * or for a list of subscription ids.
   *
   * @param {object=} params
   * @param {Array<string>=} params.events
   *     List of events to unsubscribe from.
   * @param {Array<string>=} params.contexts
   *     Optional list of top-level browsing context ids
   *     to unsubscribe the events from.
   * @param {Array<string>=} params.subscriptions
   *     List of subscription identifiers to unsubscribe from.
   *
   * @throws {InvalidArgumentError}
   *     If <var>events</var> or <var>contexts</var> are not valid types.
   */
  async unsubscribe(params = {}) {
    const { events = null, contexts = null, subscriptions = null } = params;

    const listeners =
      subscriptions === null
        ? this.#unsubscribeByAttributes(events, contexts)
        : this.#unsubscribeById(subscriptions);

    // Unsubscribe from the relevant engine-internal events.
    await this.messageHandler.eventsDispatcher.update(listeners);
  }

  #assertModuleSupportsEvent(moduleName, event) {
    const rootModuleClass = this.#getRootModuleClass(moduleName);
    if (!rootModuleClass?.supportsEvent(event)) {
      throw new lazy.error.InvalidArgumentError(
        `${event} is not a valid event name`
      );
    }
  }

  #assertNonEmptyArrayWithStrings(array, variableName) {
    lazy.assert.isNonEmptyArray(
      array,
      `Expected "${variableName}" to be a non-empty array, ` +
        lazy.pprint`got ${array}`
    );
    array.forEach(item => {
      lazy.assert.string(
        item,
        `Expected elements of "${variableName}" to be a string, ` +
          lazy.pprint`got ${item}`
      );
    });
  }

  #createListener(
    enable,
    { eventName, traversableId = null, userContextId = null }
  ) {
    let contextDescriptor;

    if (traversableId === null && userContextId === null) {
      contextDescriptor = {
        type: lazy.ContextDescriptorType.All,
      };
    } else if (userContextId !== null) {
      contextDescriptor = {
        type: lazy.ContextDescriptorType.UserContext,
        id: userContextId,
      };
    } else {
      const traversable = lazy.TabManager.getBrowsingContextById(traversableId);

      if (traversable === null) {
        return null;
      }

      contextDescriptor = {
        type: lazy.ContextDescriptorType.TopBrowsingContext,
        id: traversable.browserId,
      };
    }

    return {
      event: eventName,
      contextDescriptor,
      callback: this.#onMessageHandlerEvent,
      enable,
    };
  }

  #createListenerToSubscribe(params) {
    return this.#createListener(true, params);
  }

  #createListenerToUnsubscribe(params) {
    return this.#createListener(false, params);
  }

  /**
   * Get a set of top-level traversables for which an event is enabled.
   *
   * @see https://w3c.github.io/webdriver-bidi/#set-of-top-level-traversables-for-which-an-event-is-enabled
   *
   * @param {string} eventName
   *     The name of the event.
   *
   * @returns {Array<BrowsingContext>}
   *     The list of top-level traversables.
   */
  #getEnabledTopLevelTraversables(eventName) {
    let result = new Set();

    for (const subscription of this.#getSubscriptionsForEvent(eventName)) {
      const { topLevelTraversableIds } = subscription;

      if (this.#isSubscriptionGlobal(subscription)) {
        for (const traversable of lazy.TabManager.tabs) {
          result.add(traversable);
        }

        break;
      }

      result = this.#getNavigablesByIds(topLevelTraversableIds);
    }

    return result;
  }

  #getListenersToSubscribe(
    eventNames,
    includeGlobal,
    subscribeStepEvents,
    userContextIds
  ) {
    const listeners = [];

    for (const eventName of eventNames) {
      if (includeGlobal) {
        // Since we're going to subscribe to all top-level
        // traversable ids to not have duplicate subscriptions,
        // we have to unsubscribe from already subscribed.
        const alreadyEnabledTraversableIds =
          this.#obtainEventEnabledTraversableIds(eventName);
        for (const traversableId of alreadyEnabledTraversableIds) {
          listeners.push(
            this.#createListenerToUnsubscribe({
              eventName,
              traversableId,
            })
          );
        }

        // Also unsubscribe from already subscribed user contexts.
        const alreadyEnabledUserContextIds =
          this.#obtainEventEnabledUserContextIds(eventName);
        for (const userContextId of alreadyEnabledUserContextIds) {
          listeners.push(
            this.#createListenerToUnsubscribe({
              eventName,
              userContextId,
            })
          );
        }

        listeners.push(this.#createListenerToSubscribe({ eventName }));
      } else if (userContextIds.size !== 0) {
        for (const userContextId of userContextIds) {
          // Do nothing if the event has already a global subscription.
          if (this.#hasGlobalEventSubscription(eventName)) {
            continue;
          }

          // Since we're going to subscribe to all top-level
          // traversable ids which belongs to the certain user context
          // to not have duplicate subscriptions,
          // we have to unsubscribe from already subscribed.
          const alreadyEnabledTraversableIds =
            this.#obtainEventEnabledTraversableIds(eventName, userContextId);
          for (const traversableId of alreadyEnabledTraversableIds) {
            listeners.push(
              this.#createListenerToUnsubscribe({
                eventName,
                traversableId,
              })
            );
          }

          listeners.push(
            this.#createListenerToSubscribe({ eventName, userContextId })
          );
        }
      } else {
        for (const navigable of subscribeStepEvents.get(eventName)) {
          // Do nothing if the event has already a global subscription
          // or subscription to the associated user context.
          if (
            this.#hasGlobalEventSubscription(eventName) ||
            this.#hasSubscriptionByAssociatedUserContext(eventName, navigable)
          ) {
            continue;
          }

          const traversableId =
            lazy.TabManager.getIdForBrowsingContext(navigable);
          listeners.push(
            this.#createListenerToSubscribe({
              eventName,
              traversableId,
            })
          );
        }
      }
    }

    return listeners;
  }

  #getListenersToUnsubscribe(subscription) {
    const { eventNames, topLevelTraversableIds, userContextIds } = subscription;
    const listeners = [];

    for (const eventName of eventNames) {
      // Do nothing if there is a global subscription.
      if (this.#hasGlobalEventSubscription(eventName)) {
        continue;
      }

      if (this.#isSubscriptionGlobal(subscription)) {
        listeners.push(
          ...this.#getListenersToUnsubscribeFromGlobalSubscription(eventName)
        );
      } else if (userContextIds.size !== 0) {
        for (const userContextId of userContextIds) {
          listeners.push(
            ...this.#getListenersToUnsubscribeFromUserContext(
              eventName,
              userContextId
            )
          );
        }
      } else {
        for (const traversableId of topLevelTraversableIds) {
          listeners.push(
            this.#getListenersToUnsubscribeFromTraversable(
              eventName,
              traversableId
            )
          );
        }
      }
    }

    return listeners;
  }

  #getListenersToUnsubscribeFromGlobalSubscription(eventName) {
    // Unsubscribe from the global subscription.
    const listeners = [this.#createListenerToUnsubscribe({ eventName })];

    // Subscribe again to user contexts which have a subscription and
    // to traversables which have individual subscriptions,
    // but are not associated with subscribed user contexts.
    for (const item of this.#getSubscriptionsForEvent(eventName)) {
      for (const userContextId of item.userContextIds) {
        listeners.push(
          this.#createListenerToSubscribe({
            eventName,
            userContextId,
          })
        );
      }

      for (const traversableId of item.topLevelTraversableIds) {
        const traversable =
          lazy.TabManager.getBrowsingContextById(traversableId);

        // Do nothing if traversable doesn't exist anymore or
        // there is already a subscription to the associated user context.
        if (
          traversable === null ||
          this.#hasSubscriptionByAssociatedUserContext(eventName, traversable)
        ) {
          continue;
        }

        listeners.push(
          this.#createListenerToSubscribe({
            eventName,
            traversableId,
          })
        );
      }
    }

    return listeners;
  }

  #getListenersToUnsubscribeFromTraversable(eventName, traversableId) {
    // Do nothing if traversable is already closed or still has another subscription.
    const traversable = lazy.TabManager.getBrowsingContextById(traversableId);
    if (
      traversable === null ||
      this.#hasSubscriptionByAssociatedUserContext(eventName, traversable) ||
      this.#hasSubscriptionByTraversableId(eventName, traversableId)
    ) {
      return null;
    }

    return this.#createListenerToUnsubscribe({
      eventName,
      traversableId,
    });
  }

  #getListenersToUnsubscribeFromUserContext(eventName, userContextId) {
    // Do nothing if there is another subscription for this user context.
    if (this.#hasSubscriptionByUserContextId(eventName, userContextId)) {
      return [];
    }

    // Unsubscribe from the user context.
    const listeners = [
      this.#createListenerToUnsubscribe({ eventName, userContextId }),
    ];

    // Resubscribe to traversables which are associated with this user context and
    // have individual subscriptions.
    const alreadyEnabledTraversableIds = this.#obtainEventEnabledTraversableIds(
      eventName,
      userContextId
    );
    for (const traversableId of alreadyEnabledTraversableIds) {
      listeners.push(
        this.#createListenerToSubscribe({
          eventName,
          traversableId,
        })
      );
    }

    return listeners;
  }

  /**
   * Retrieves a navigable based on its id.
   *
   * @see https://w3c.github.io/webdriver-bidi/#get-a-navigable
   *
   * @param {number} navigableId
   *     Id of the navigable.
   *
   * @returns {BrowsingContext=}
   *     The navigable or null if <var>navigableId</var> is null.
   * @throws {NoSuchFrameError}
   *     If the navigable cannot be found.
   */
  #getNavigable(navigableId) {
    if (navigableId === null) {
      return null;
    }

    const navigable = lazy.TabManager.getBrowsingContextById(navigableId);
    if (!navigable) {
      throw new lazy.error.NoSuchFrameError(
        `Browsing context with id ${navigableId} not found`
      );
    }

    return navigable;
  }

  /**
   * Get a list of navigables by provided ids.
   *
   * @see https://w3c.github.io/webdriver-bidi/#get-navigables-by-ids
   *
   * @param {Set<string>} navigableIds
   *     The set of the navigable ids.
   *
   * @returns {Set<BrowsingContext>}
   *     The set of navigables.
   */
  #getNavigablesByIds(navigableIds) {
    const result = new Set();

    for (const navigableId of navigableIds) {
      const navigable = lazy.TabManager.getBrowsingContextById(navigableId);

      if (navigable !== null) {
        result.add(navigable);
      }
    }

    return result;
  }

  #getRootModuleClass(moduleName) {
    // Modules which support event subscriptions should have a root module
    // defining supported events.
    const rootDestination = { type: lazy.RootMessageHandler.type };
    const moduleClasses = this.messageHandler.getAllModuleClasses(
      moduleName,
      rootDestination
    );

    if (!moduleClasses.length) {
      throw new lazy.error.InvalidArgumentError(
        `Module ${moduleName} does not exist`
      );
    }

    return moduleClasses[0];
  }

  #getSubscriptionsForEvent(eventName) {
    return this.#subscriptions.filter(({ eventNames }) =>
      eventNames.has(eventName)
    );
  }

  #getTopLevelTraversableContextIds(contextIds) {
    const topLevelTraversableContextIds = new Set();
    const inputContextIds = new Set(contextIds);

    if (inputContextIds.size !== 0) {
      const navigables = this.#getValidNavigablesByIds(inputContextIds);
      const topLevelTraversable = this.#getTopLevelTraversables(navigables);

      for (const navigable of topLevelTraversable) {
        topLevelTraversableContextIds.add(
          lazy.TabManager.getIdForBrowsingContext(navigable)
        );
      }
    }

    return topLevelTraversableContextIds;
  }

  /**
   * Get a list of top-level traversables for provided navigables.
   *
   * @see https://w3c.github.io/webdriver-bidi/#get-top-level-traversables
   *
   * @param {Array<BrowsingContext>} navigables
   *     The list of the navigables.
   *
   * @returns {Set<BrowsingContext>}
   *     The set of top-level traversables.
   */
  #getTopLevelTraversables(navigables) {
    const result = new Set();

    for (const { top } of navigables) {
      result.add(top);
    }

    return result;
  }

  /**
   * Get a list of valid navigables by provided ids.
   *
   * @see https://w3c.github.io/webdriver-bidi/#get-valid-navigables-by-ids
   *
   * @param {Set<string>} navigableIds
   *     The set of the navigable ids.
   *
   * @returns {Set<BrowsingContext>}
   *     The set of navigables.
   * @throws {NoSuchFrameError}
   *     If the navigable cannot be found.
   */
  #getValidNavigablesByIds(navigableIds) {
    const result = new Set();

    for (const navigableId of navigableIds) {
      result.add(this.#getNavigable(navigableId));
    }

    return result;
  }

  #hasGlobalEventSubscription(eventName) {
    for (const subscription of this.#getSubscriptionsForEvent(eventName)) {
      if (this.#isSubscriptionGlobal(subscription)) {
        return true;
      }
    }

    return false;
  }

  // Check if for a given event name and traversable there is
  // a subscription for a user context associated with this traversable.
  #hasSubscriptionByAssociatedUserContext(eventName, traversable) {
    if (traversable === null) {
      return false;
    }

    return this.#hasSubscriptionByUserContextId(
      eventName,
      traversable.originAttributes.userContextId
    );
  }

  #hasSubscriptionByTraversableId(eventName, traversableId) {
    for (const subscription of this.#getSubscriptionsForEvent(eventName)) {
      const { topLevelTraversableIds } = subscription;

      for (const topLevelTraversableId of topLevelTraversableIds) {
        if (topLevelTraversableId === traversableId) {
          return true;
        }
      }
    }

    return false;
  }

  #hasSubscriptionByUserContextId(eventName, userContextId) {
    for (const subscription of this.#getSubscriptionsForEvent(eventName)) {
      const { userContextIds } = subscription;

      if (userContextIds.has(userContextId)) {
        return true;
      }
    }

    return false;
  }

  /**
   * Identify if a provided subscription is global.
   *
   * @see https://w3c.github.io/webdriver-bidi/#subscription-global
   *
   * @param {Subscription} subscription
   *     A subscription object.
   *
   * @returns {boolean}
   *     Return true if the subscription is global, false otherwise.
   */
  #isSubscriptionGlobal(subscription) {
    return (
      subscription.topLevelTraversableIds.size === 0 &&
      subscription.userContextIds.size === 0
    );
  }

  /**
   * Obtain a list of event enabled traversable ids.
   *
   * @param {string} eventName
   *     The name of the event.
   * @param {string=} userContextId
   *     The user context id.
   *
   * @returns {Set<string>}
   *     The set of traversable ids.
   */
  #obtainEventEnabledTraversableIds(eventName, userContextId = null) {
    let traversableIds = new Set();

    for (const { topLevelTraversableIds } of this.#getSubscriptionsForEvent(
      eventName
    )) {
      if (topLevelTraversableIds.size === 0) {
        continue;
      }

      if (userContextId === null) {
        traversableIds = traversableIds.union(topLevelTraversableIds);
        continue;
      }

      for (const traversableId of topLevelTraversableIds) {
        const traversable =
          lazy.TabManager.getBrowsingContextById(traversableId);

        if (traversable === null) {
          continue;
        }

        if (traversable.originAttributes.userContextId === userContextId) {
          traversableIds.add(traversableId);
        }
      }
    }

    return traversableIds;
  }

  #obtainEventEnabledUserContextIds(eventName) {
    let enabledUserContextIds = new Set();

    for (const { userContextIds } of this.#getSubscriptionsForEvent(
      eventName
    )) {
      enabledUserContextIds = enabledUserContextIds.union(userContextIds);
    }

    return enabledUserContextIds;
  }

  /**
   * Obtain a set of events based on the given event name.
   *
   * Could contain a period for a specific event,
   * or just the module name for all events.
   *
   * @param {string} event
   *     Name of the event to process.
   *
   * @returns {Set<string>}
   *     A Set with the expanded events in the form of `<module>.<event>`.
   *
   * @throws {InvalidArgumentError}
   *     If <var>event</var> does not reference a valid event.
   */
  #obtainEvents(event) {
    const events = new Set();

    // Check if a period is present that splits the event name into the module,
    // and the actual event. Hereby only care about the first found instance.
    const index = event.indexOf(".");
    if (index >= 0) {
      const [moduleName] = event.split(".");
      this.#assertModuleSupportsEvent(moduleName, event);
      events.add(event);
    } else {
      // Interpret the name as module, and register all its available events
      const rootModuleClass = this.#getRootModuleClass(event);
      const supportedEvents = rootModuleClass?.supportedEvents;

      for (const eventName of supportedEvents) {
        events.add(eventName);
      }
    }

    return events;
  }

  #onMessageHandlerEvent = (name, event) => {
    this.messageHandler.emitProtocolEvent(name, event);
  };

  #unsubscribeByAttributes(events, contextIds) {
    const listeners = [];

    // Check input types until we run schema validation.
    this.#assertNonEmptyArrayWithStrings(events, "events");
    if (contextIds !== null) {
      this.#assertNonEmptyArrayWithStrings(contextIds, "contexts");
    }

    const eventNames = new Set();
    events.forEach(name => {
      this.#obtainEvents(name).forEach(event => eventNames.add(event));
    });

    const topLevelTraversableContextIds =
      this.#getTopLevelTraversableContextIds(contextIds);

    const newSubscriptions = [];
    const matchedEvents = new Set();
    const matchedContexts = new Set();

    for (const subscription of this.#subscriptions) {
      // Keep subscription if it doesn't contain any target events.
      if (subscription.eventNames.intersection(eventNames).size === 0) {
        newSubscriptions.push(subscription);
        continue;
      }

      // Unsubscribe globally.
      if (topLevelTraversableContextIds.size === 0) {
        // Keep subscription if verified subscription is not global.
        if (!this.#isSubscriptionGlobal(subscription)) {
          newSubscriptions.push(subscription);
          continue;
        }

        // Delete event names from the subscription.
        const subscriptionEventNames = new Set(subscription.eventNames);
        for (const eventName of eventNames) {
          if (subscriptionEventNames.has(eventName)) {
            matchedEvents.add(eventName);
            subscriptionEventNames.delete(eventName);

            listeners.push(this.#createListenerToUnsubscribe({ eventName }));
          }
        }

        // If the subscription still contains some event,
        // save a new partial subscription.
        if (subscriptionEventNames.size !== 0) {
          const clonedSubscription = {
            subscriptionId: subscription.subscriptionId,
            eventNames: new Set(subscriptionEventNames),
            topLevelTraversableIds: new Set(),
            userContextIds: new Set(subscription.userContextIds),
          };
          newSubscriptions.push(clonedSubscription);
        }
      }
      // Keep the subscription if it's global but we want to unsubscribe only from some contexts.
      else if (this.#isSubscriptionGlobal(subscription)) {
        newSubscriptions.push(subscription);
      } else {
        // Map with an event name as a key and the set of subscribed traversable ids as a value.
        const eventMap = new Map();

        // Populate the map.
        for (const eventName of subscription.eventNames) {
          eventMap.set(eventName, new Set(subscription.topLevelTraversableIds));
        }

        for (const eventName of eventNames) {
          // Skip if there is no subscription related to this event.
          if (!eventMap.has(eventName)) {
            continue;
          }

          for (const topLevelTraversableId of topLevelTraversableContextIds) {
            // Skip if there is no subscription related to this event and this traversable id.
            if (!eventMap.get(eventName).has(topLevelTraversableId)) {
              continue;
            }

            matchedContexts.add(topLevelTraversableId);
            matchedEvents.add(eventName);
            eventMap.get(eventName).delete(topLevelTraversableId);

            listeners.push(
              this.#createListenerToUnsubscribe({
                eventName,
                traversableId: topLevelTraversableId,
              })
            );
          }

          if (eventMap.get(eventName).size === 0) {
            eventMap.delete(eventName);
          }
        }

        // Build new partial subscriptions based on the remaining data in eventMap.
        for (const [
          eventName,
          remainingTopLevelTraversableIds,
        ] of eventMap.entries()) {
          const partialSubscription = {
            subscriptionId: subscription.subscriptionId,
            eventNames: new Set([eventName]),
            topLevelTraversableIds: remainingTopLevelTraversableIds,
            userContextIds: new Set(subscription.userContextIds),
          };

          newSubscriptions.push(partialSubscription);

          const traversableIdsToUnsubscribe =
            subscription.topLevelTraversableIds.difference(
              remainingTopLevelTraversableIds
            );

          for (const traversableId of traversableIdsToUnsubscribe) {
            listeners.push(
              this.#createListenerToUnsubscribe({ eventName, traversableId })
            );
          }
        }
      }
    }

    if (matchedEvents.symmetricDifference(eventNames).size > 0) {
      throw new lazy.error.InvalidArgumentError(
        `Failed to unsubscribe from events: ${Array.from(eventNames).join(", ")}`
      );
    }
    if (
      topLevelTraversableContextIds.size > 0 &&
      matchedContexts.symmetricDifference(topLevelTraversableContextIds).size >
        0
    ) {
      throw new lazy.error.InvalidArgumentError(
        `Failed to unsubscribe from events: ${Array.from(eventNames).join(", ")} for context ids: ${Array.from(topLevelTraversableContextIds).join(", ")}`
      );
    }

    this.#subscriptions = newSubscriptions;

    return listeners;
  }

  #unsubscribeById(subscriptionIds) {
    this.#assertNonEmptyArrayWithStrings(subscriptionIds, "subscriptions");

    const subscriptions = new Set(subscriptionIds);
    const unknownSubscriptionIds = subscriptions.difference(
      this.#knownSubscriptionIds
    );

    if (unknownSubscriptionIds.size !== 0) {
      throw new lazy.error.InvalidArgumentError(
        `Failed to unsubscribe from subscriptions with ids: ${Array.from(subscriptionIds).join(", ")} ` +
          `(unknown ids: ${Array.from(unknownSubscriptionIds).join(", ")})`
      );
    }

    const listeners = [];
    const subscriptionIdsToRemove = new Set();
    const subscriptionsToRemove = new Set();

    for (const subscription of this.#subscriptions) {
      const { subscriptionId } = subscription;

      if (!subscriptions.has(subscriptionId)) {
        continue;
      }

      subscriptionIdsToRemove.add(subscriptionId);
      subscriptionsToRemove.add(subscription);
    }

    this.#knownSubscriptionIds =
      this.#knownSubscriptionIds.difference(subscriptions);
    this.#subscriptions = this.#subscriptions.filter(
      ({ subscriptionId }) => !subscriptionIdsToRemove.has(subscriptionId)
    );

    for (const subscription of subscriptionsToRemove) {
      listeners.push(...this.#getListenersToUnsubscribe(subscription));
    }

    return listeners;
  }
}

// To export the class as lower-case
export const session = SessionModule;
