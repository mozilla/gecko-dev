/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * JS module implementation of setTimeout and clearTimeout.
 */

// This gives us >=2^30 unique timer IDs, enough for 1 per ms for 12.4 days.
var gNextId = 1; // setTimeout and setInterval must return a positive integer

/**
 * @type {Map<number, nsITimer>}
 */
var gTimerTable = new Map();

/**
 * @type {Map<number, (deadline: IdleDeadline) => void>}
 */
var gIdleTable = new Map();

// Don't generate this for every timer.
var setTimeout_timerCallbackQI = ChromeUtils.generateQI([
  "nsITimerCallback",
  "nsINamed",
]);

/**
 * @template {any[]} T
 *
 * @param {(...args: T) => any} callback
 * @param {number} milliseconds
 * @param {boolean} [isInterval]
 * @param {nsIEventTarget} [target]
 * @param {T} [args]
 */
function _setTimeoutOrIsInterval(
  callback,
  milliseconds,
  isInterval,
  target,
  args
) {
  if (typeof callback !== "function") {
    throw new Error(
      `callback is not a function in ${
        isInterval ? "setInterval" : "setTimeout"
      }`
    );
  }
  let id = gNextId++;
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

  if (target) {
    timer.target = target;
  }

  let callbackObj = {
    QueryInterface: setTimeout_timerCallbackQI,

    // nsITimerCallback
    notify() {
      if (!isInterval) {
        gTimerTable.delete(id);
      }
      callback.apply(null, args);
    },

    // nsINamed
    get name() {
      return `${
        isInterval ? "setInterval" : "setTimeout"
      }() for ${Cu.getDebugName(callback)}`;
    },
  };

  timer.initWithCallback(
    callbackObj,
    milliseconds,
    isInterval ? timer.TYPE_REPEATING_SLACK : timer.TYPE_ONE_SHOT
  );

  gTimerTable.set(id, timer);
  return id;
}

/**
 * Sets a timeout.
 *
 * @template {any[]} T
 *
 * @param {(...args: T) => any} callback
 * @param {number} milliseconds
 * @param {T} [args]
 */
export function setTimeout(callback, milliseconds, ...args) {
  return _setTimeoutOrIsInterval(callback, milliseconds, false, null, args);
}

/**
 * Sets a timeout with a given event target.
 *
 * @template {any[]} T
 *
 * @param {(...args: T) => any} callback
 * @param {number} milliseconds
 * @param {nsIEventTarget} target
 * @param {T} [args]
 */
export function setTimeoutWithTarget(callback, milliseconds, target, ...args) {
  return _setTimeoutOrIsInterval(callback, milliseconds, false, target, args);
}

/**
 * Sets an interval timer.
 *
 * @template {any[]} T
 *
 * @param {(...args: T) => any} callback
 * @param {number} milliseconds
 * @param {T} [args]
 */
export function setInterval(callback, milliseconds, ...args) {
  return _setTimeoutOrIsInterval(callback, milliseconds, true, null, args);
}

/**
 * Sets an interval timer.
 *
 * @template {any[]} T
 *
 * @param {(...args: T) => any} callback
 * @param {number} milliseconds
 * @param {nsIEventTarget} target
 * @param {T} [args]
 */
export function setIntervalWithTarget(callback, milliseconds, target, ...args) {
  return _setTimeoutOrIsInterval(callback, milliseconds, true, target, args);
}

/**
 * Clears the given timer.
 *
 * @param {number} id
 */
function clear(id) {
  if (gTimerTable.has(id)) {
    gTimerTable.get(id).cancel();
    gTimerTable.delete(id);
  }
}

/**
 * Clears the given timer.
 */
export var clearInterval = clear;

/**
 * Clears the given timer.
 */
export var clearTimeout = clear;

/**
 * Dispatches the given callback to the main thread when it would be otherwise
 * idle. The callback may be canceled via `cancelIdleCallback` - the idle
 * dispatch will still happen but it won't be called.
 *
 * @param {(deadline: IdleDeadline) => any} callback
 * @param {object} options
 */
export function requestIdleCallback(callback, options) {
  if (typeof callback !== "function") {
    throw new Error("callback is not a function in requestIdleCallback");
  }
  let id = gNextId++;

  ChromeUtils.idleDispatch(deadline => {
    if (gIdleTable.has(id)) {
      gIdleTable.delete(id);
      callback(deadline);
    }
  }, options);
  gIdleTable.set(id, callback);
  return id;
}

/**
 * Cancels the given idle callback
 *
 * @param {number} id
 */
export function cancelIdleCallback(id) {
  if (gIdleTable.has(id)) {
    gIdleTable.delete(id);
  }
}
