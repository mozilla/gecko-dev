/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Redux store object wrapper to temporarily disable the store update notifications
 * while the panel is hidden. The Redux actions are still dispatched and the reducers
 * will be updating, but the changes are not communicated to connected components.
 * i.e. redux's `connect` method won't be calling `mapStateToProps` function of all connected components.
 */
function visibilityHandlerStore(store) {
  return {
    /**
     * Override to pause calling `listener` function while the panel is hidden.
     * The function will be called once when the panel is shown.
     *
     * @param {Function} listener
     * @param {Object} options
     * @param {Boolean} options.ignoreVisibility
     *        If true, bypass this helper to listen to all store updates,
     *        regarless of panel visibility. This is useful for tests.
     */
    subscribe(listener, { ignoreVisibility = false } = {}) {
      // Test may pass a custom flag to ignore the visibility handler and listener
      // to all state changes regarless of panel's visibility.
      if (ignoreVisibility) {
        return store.subscribe(listener);
      }

      function onVisibilityChange() {
        if (document.visibilityState == "visible") {
          // Force an update to resume updates when the panel becomes visible again
          listener();
        }
      }
      document.addEventListener("visibilitychange", onVisibilityChange);
      const unsubscribe = store.subscribe(function () {
        // This is the key operation of this class, to prevent notification
        // when the panel is hidden.
        if (document.visibilityState == "visible") {
          listener();
        }
      });

      // Calling `subscribe` should return an unsubscribe function
      return function () {
        unsubscribe();
        document.removeEventListener("visibilitychange", onVisibilityChange);
      };
    },

    // Provide expected default store functions
    getState() {
      return store.getState();
    },
    dispatch(action) {
      return store.dispatch(action);
    },
  };
}

exports.visibilityHandlerStore = visibilityHandlerStore;
