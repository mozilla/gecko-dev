/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const {
  error,
  stack,
  TimeoutError,
} = ChromeUtils.import("chrome://marionette/content/error.js", {});
const {Log} = ChromeUtils.import("chrome://marionette/content/log.js", {});

XPCOMUtils.defineLazyGetter(this, "log", Log.get);

this.EXPORTED_SYMBOLS = [
  "DebounceCallback",
  "IdlePromise",
  "MessageManagerDestroyedPromise",
  "PollPromise",
  "Sleep",
  "TimedPromise",
];

const {TYPE_ONE_SHOT, TYPE_REPEATING_SLACK} = Ci.nsITimer;

const PROMISE_TIMEOUT = AppConstants.DEBUG ? 4500 : 1500;

/**
 * @callback Condition
 *
 * @param {function(*)} resolve
 *     To be called when the condition has been met.  Will return the
 *     resolved value.
 * @param {function} reject
 *     To be called when the condition has not been met.  Will cause
 *     the condition to be revaluated or time out.
 *
 * @return {*}
 *     The value from calling ``resolve``.
 */

/**
 * Runs a Promise-like function off the main thread until it is resolved
 * through ``resolve`` or ``rejected`` callbacks.  The function is
 * guaranteed to be run at least once, irregardless of the timeout.
 *
 * The ``func`` is evaluated every ``interval`` for as long as its
 * runtime duration does not exceed ``interval``.  Evaluations occur
 * sequentially, meaning that evaluations of ``func`` are queued if
 * the runtime evaluation duration of ``func`` is greater than ``interval``.
 *
 * ``func`` is given two arguments, ``resolve`` and ``reject``,
 * of which one must be called for the evaluation to complete.
 * Calling ``resolve`` with an argument indicates that the expected
 * wait condition was met and will return the passed value to the
 * caller.  Conversely, calling ``reject`` will evaluate ``func``
 * again until the ``timeout`` duration has elapsed or ``func`` throws.
 * The passed value to ``reject`` will also be returned to the caller
 * once the wait has expired.
 *
 * Usage::
 *
 *     let els = new PollPromise((resolve, reject) => {
 *       let res = document.querySelectorAll("p");
 *       if (res.length > 0) {
 *         resolve(Array.from(res));
 *       } else {
 *         reject([]);
 *       }
 *     });
 *
 * @param {Condition} func
 *     Function to run off the main thread.
 * @param {number=} [timeout=2000] timeout
 *     Desired timeout.  If 0 or less than the runtime evaluation
 *     time of ``func``, ``func`` is guaranteed to run at least once.
 *     The default is 2000 milliseconds.
 * @param {number=} [interval=10] interval
 *     Duration between each poll of ``func`` in milliseconds.
 *     Defaults to 10 milliseconds.
 *
 * @return {Promise.<*>}
 *     Yields the value passed to ``func``'s
 *     ``resolve`` or ``reject`` callbacks.
 *
 * @throws {*}
 *     If ``func`` throws, its error is propagated.
 * @throws {TypeError}
 *     If `timeout` or `interval`` are not numbers.
 * @throws {RangeError}
 *     If `timeout` or `interval` are not unsigned integers.
 */
function PollPromise(func, {timeout = 2000, interval = 10} = {}) {
  const timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

  if (typeof func != "function") {
    throw new TypeError();
  }
  if (!(typeof timeout == "number" && typeof interval == "number")) {
    throw new TypeError();
  }
  if ((!Number.isInteger(timeout) || timeout < 0) ||
      (!Number.isInteger(interval) || interval < 0)) {
    throw new RangeError();
  }

  return new Promise((resolve, reject) => {
    const start = new Date().getTime();
    const end = start + timeout;

    let evalFn = () => {
      new Promise(func).then(resolve, rejected => {
        if (error.isError(rejected)) {
          throw rejected;
        }

        // return if timeout is 0, allowing |func| to be evaluated at
        // least once
        if (start == end || new Date().getTime() >= end) {
          resolve(rejected);
        }
      }).catch(reject);
    };

    // the repeating slack timer waits |interval|
    // before invoking |evalFn|
    evalFn();

    timer.init(evalFn, interval, TYPE_REPEATING_SLACK);

  }).then(res => {
    timer.cancel();
    return res;
  }, err => {
    timer.cancel();
    throw err;
  });
}

/**
 * Represents the timed, eventual completion (or failure) of an
 * asynchronous operation, and its resulting value.
 *
 * In contrast to a regular Promise, it times out after ``timeout``.
 *
 * @param {Condition} func
 *     Function to run, which will have its ``reject``
 *     callback invoked after the ``timeout`` duration is reached.
 *     It is given two callbacks: ``resolve(value)`` and
 *     ``reject(error)``.
 * @param {timeout=} timeout
 *     ``condition``'s ``reject`` callback will be called
 *     after this timeout, given in milliseconds.
 *     By default 1500 ms in an optimised build and 4500 ms in
 *     debug builds.
 * @param {Error=} [throws=TimeoutError] throws
 *     When the ``timeout`` is hit, this error class will be
 *     thrown.  If it is null, no error is thrown and the promise is
 *     instead resolved on timeout.
 *
 * @return {Promise.<*>}
 *     Timed promise.
 *
 * @throws {TypeError}
 *     If `timeout` is not a number.
 * @throws {RangeError}
 *     If `timeout` is not an unsigned integer.
 */
function TimedPromise(fn,
    {timeout = PROMISE_TIMEOUT, throws = TimeoutError} = {}) {
  const timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

  if (typeof fn != "function") {
    throw new TypeError();
  }
  if (typeof timeout != "number") {
    throw new TypeError();
  }
  if (!Number.isInteger(timeout) || timeout < 0) {
    throw new RangeError();
  }

  return new Promise((resolve, reject) => {
    // Reject only if |throws| is given.  Otherwise it is assumed that
    // the user is OK with the promise timing out.
    let bail = () => {
      if (throws !== null) {
        let err = new throws();
        reject(err);
      } else {
        log.warn(`TimedPromise timed out after ${timeout} ms`, stack());
        resolve();
      }
    };

    timer.initWithCallback({notify: bail}, timeout, TYPE_ONE_SHOT);

    try {
      fn(resolve, reject);
    } catch (e) {
      reject(e);
    }

  }).then(res => {
    timer.cancel();
    return res;
  }, err => {
    timer.cancel();
    throw err;
  });
}

/**
 * Pauses for the given duration.
 *
 * @param {number} timeout
 *     Duration to wait before fulfilling promise in milliseconds.
 *
 * @return {Promise}
 *     Promise that fulfills when the `timeout` is elapsed.
 *
 * @throws {TypeError}
 *     If `timeout` is not a number.
 * @throws {RangeError}
 *     If `timeout` is not an unsigned integer.
 */
function Sleep(timeout) {
  if (typeof timeout != "number") {
    throw new TypeError();
  }
  return new TimedPromise(() => {}, {timeout, throws: null});
}

/**
 * Detects when the specified message manager has been destroyed.
 *
 * One can observe the removal and detachment of a content browser
 * (`<xul:browser>`) or a chrome window by its message manager
 * disconnecting.
 *
 * When a browser is associated with a tab, this is safer than only
 * relying on the event `TabClose` which signalises the _intent to_
 * remove a tab and consequently would lead to the destruction of
 * the content browser and its browser message manager.
 *
 * When closing a chrome window it is safer than only relying on
 * the event 'unload' which signalises the _intent to_ close the
 * chrome window and consequently would lead to the destruction of
 * the window and its window message manager.
 *
 * @param {MessageListenerManager} messageManager
 *     The message manager to observe for its disconnect state.
 *     Use the browser message manager when closing a content browser,
 *     and the window message manager when closing a chrome window.
 *
 * @return {Promise}
 *     A promise that resolves when the message manager has been destroyed.
 */
function MessageManagerDestroyedPromise(messageManager) {
  return new Promise(resolve => {
    function observe(subject, topic) {
      log.trace(`Received observer notification ${topic}`);

      if (subject == messageManager) {
        Services.obs.removeObserver(this, "message-manager-disconnect");
        resolve();
      }
    }

    Services.obs.addObserver(observe, "message-manager-disconnect");
  });
}

/**
 * Throttle until the main thread is idle and `window` has performed
 * an animation frame (in that order).
 *
 * @param {ChromeWindow} win
 *     Window to request the animation frame from.
 *
 * @return Promise
 */
function IdlePromise(win) {
  return new Promise(resolve => {
    Services.tm.idleDispatchToMainThread(() => {
      win.requestAnimationFrame(resolve);
    });
  });
}

/**
 * Wraps a callback function, that, as long as it continues to be
 * invoked, will not be triggered.  The given function will be
 * called after the timeout duration is reached, after no more
 * events fire.
 *
 * This class implements the {@link EventListener} interface,
 * which means it can be used interchangably with `addEventHandler`.
 *
 * Debouncing events can be useful when dealing with e.g. DOM events
 * that fire at a high rate.  It is generally advisable to avoid
 * computationally expensive operations such as DOM modifications
 * under these circumstances.
 *
 * One such high frequenecy event is `resize` that can fire multiple
 * times before the window reaches its final dimensions.  In order
 * to delay an operation until the window has completed resizing,
 * it is possible to use this technique to only invoke the callback
 * after the last event has fired::
 *
 *     let cb = new DebounceCallback(event => {
 *       // fires after the final resize event
 *       console.log("resize", event);
 *     });
 *     window.addEventListener("resize", cb);
 *
 * Note that it is not possible to use this synchronisation primitive
 * with `addEventListener(..., {once: true})`.
 *
 * @param {function(Event)} fn
 *     Callback function that is guaranteed to be invoked once only,
 *     after `timeout`.
 * @param {number=} [timeout = 250] timeout
 *     Time since last event firing, before `fn` will be invoked.
 */
class DebounceCallback {
  constructor(fn, {timeout = 250} = {}) {
    if (typeof fn != "function" || typeof timeout != "number") {
      throw new TypeError();
    }
    if (!Number.isInteger(timeout) || timeout < 0) {
      throw new RangeError();
    }

    this.fn = fn;
    this.timeout = timeout;
    this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  }

  handleEvent(ev) {
    this.timer.cancel();
    this.timer.initWithCallback(() => {
      this.timer.cancel();
      this.fn(ev);
    }, this.timeout, TYPE_ONE_SHOT);
  }
}
this.DebounceCallback = DebounceCallback;
