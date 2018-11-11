/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ./shared-head.js */
// Currently this file expects "defer" to be imported into scope.

// Common utility functions for working with Redux stores.  The file is meant
// to be safe to load in both mochitest and xpcshell environments.

/**
 * A logging function that can be used from xpcshell and browser mochitest
 * environments.
 */
function commonLog(message) {
  let log;
  if (Services && Services.appinfo && Services.appinfo.name &&
      Services.appinfo.name == "Firefox") {
    log = info;
  } else {
    log = do_print;
  }
  log(message);
}

/**
 * Wait until the store has reached a state that matches the predicate.
 * @param Store store
 *        The Redux store being used.
 * @param function predicate
 *        A function that returns true when the store has reached the expected
 *        state.
 * @return Promise
 *         Resolved once the store reaches the expected state.
 */
function waitUntilState(store, predicate) {
  let deferred = defer();
  let unsubscribe = store.subscribe(check);

  commonLog(`Waiting for state predicate "${predicate}"`);
  function check() {
    if (predicate(store.getState())) {
      commonLog(`Found state predicate "${predicate}"`);
      unsubscribe();
      deferred.resolve();
    }
  }

  // Fire the check immediately in case the action has already occurred
  check();

  return deferred.promise;
}

/**
 * Wait until a particular action has been emitted by the store.
 * @param Store store
 *        The Redux store being used.
 * @param string actionType
 *        The expected action to wait for.
 * @return Promise
 *         Resolved once the expected action is emitted by the store.
 */
function waitUntilAction(store, actionType) {
  let deferred = defer();
  let unsubscribe = store.subscribe(check);
  let history = store.history;
  let index = history.length;

  commonLog(`Waiting for action "${actionType}"`);
  function check() {
    let action = history[index++];
    if (action && action.type === actionType) {
      commonLog(`Found action "${actionType}"`);
      unsubscribe();
      deferred.resolve(store.getState());
    }
  }

  return deferred.promise;
}
