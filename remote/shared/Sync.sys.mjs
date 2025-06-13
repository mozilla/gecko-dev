/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",

  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
});

const { TYPE_ONE_SHOT, TYPE_REPEATING_SLACK } = Ci.nsITimer;

const PROMISE_TIMEOUT = AppConstants.DEBUG ? 4500 : 1500;

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.REMOTE_AGENT)
);

/**
 * Throttle until the `window` has performed an animation frame.
 *
 * The animation frame is requested after the main thread has processed
 * all the already queued-up runnables.
 *
 * @param {ChromeWindow} win
 *     Window to request the animation frame from.
 * @param {object=} options
 * @param {number=} options.timeout
 *     Timeout duration in milliseconds.
 *     This copes with navigating away from hidden iframes: if
 *     fragmentNavigated happens before their animation finishes, this would
 *     never resolve otherwise. By default 1500 ms in an optimised build and
 *     4500 ms in debug builds. Specify null to disable the timeout.
 *
 * @returns {Promise}
 *
 * @throws {TypeError}
 * @throws {RangeError}
 */
export function AnimationFramePromise(win, options = {}) {
  const { timeout = PROMISE_TIMEOUT } = options;

  if (timeout !== null) {
    if (typeof timeout != "number") {
      throw new TypeError("timeout must be a number or null");
    }

    if (!Number.isInteger(timeout) || timeout < 0) {
      throw new RangeError("timeout must be a non-negative integer");
    }
  }

  const animationFramePromise = new Promise(resolve => {
    executeSoon(() => {
      win.requestAnimationFrame(resolve);
    });
  });

  const promises = [
    animationFramePromise,
    new EventPromise(win, "pagehide"), // window closed or moved to BFCache
  ];

  let timer;
  if (timeout != null) {
    promises.push(
      new Promise(resolve => {
        timer = lazy.setTimeout(() => {
          lazy.logger.warn("Timed out waiting for animation frame");
          resolve();
        }, timeout);
      })
    );
  }

  return Promise.race(promises).then(() => lazy.clearTimeout(timer));
}

/**
 * Create a helper object to defer a promise.
 *
 * @returns {object}
 *     An object that returns the following properties:
 *       - fulfilled Flag that indicates that the promise got resolved
 *       - pending Flag that indicates a not yet fulfilled/rejected promise
 *       - promise The actual promise
 *       - reject Callback to reject the promise
 *       - rejected Flag that indicates that the promise got rejected
 *       - resolve Callback to resolve the promise
 */
export function Deferred() {
  const deferred = {};

  deferred.promise = new Promise((resolve, reject) => {
    deferred.fulfilled = false;
    deferred.pending = true;
    deferred.rejected = false;

    deferred.resolve = (...args) => {
      deferred.fulfilled = true;
      deferred.pending = false;
      resolve(...args);
    };

    deferred.reject = (...args) => {
      deferred.pending = false;
      deferred.rejected = true;
      reject(...args);
    };
  });

  return deferred;
}

/**
 * Wait for an event to be fired on a specified element.
 *
 * The returned promise is guaranteed to not resolve before the
 * next event tick after the event listener is called, so that all
 * other event listeners for the element are executed before the
 * handler is executed.  For example:
 *
 *     const promise = new EventPromise(element, "myEvent");
 *     // same event tick here
 *     await promise;
 *     // next event tick here
 *
 * @param {Element} subject
 *     The element that should receive the event.
 * @param {string} eventName
 *     Case-sensitive string representing the event name to listen for.
 * @param {object=} options
 * @param {boolean=} options.capture
 *     Indicates the event will be despatched to this subject,
 *     before it bubbles down to any EventTarget beneath it in the
 *     DOM tree. Defaults to false.
 * @param {Function=} options.checkFn
 *     Called with the Event object as argument, should return true if the
 *     event is the expected one, or false if it should be ignored and
 *     listening should continue. If not specified, the first event with
 *     the specified name resolves the returned promise. Defaults to null.
 * @param {number=} options.timeout
 *     Timeout duration in milliseconds, if provided.
 *     If specified, then the returned promise will be rejected with
 *     TimeoutError, if not already resolved, after this duration has elapsed.
 *     If not specified, then no timeout is used. Defaults to null.
 * @param {boolean=} options.mozSystemGroup
 *     Determines whether to add listener to the system group. Defaults to
 *     false.
 * @param {boolean=} options.wantUntrusted
 *     Receive synthetic events despatched by web content. Defaults to false.
 *
 * @returns {Promise<Event>}
 *     Either fulfilled with the first described event, satisfying
 *     options.checkFn if specified, or rejected with TimeoutError after
 *     options.timeout milliseconds if specified.
 *
 * @throws {TypeError}
 * @throws {RangeError}
 */
export function EventPromise(subject, eventName, options = {}) {
  const {
    capture = false,
    checkFn = null,
    timeout = null,
    mozSystemGroup = false,
    wantUntrusted = false,
  } = options;
  if (
    !subject ||
    !("addEventListener" in subject) ||
    typeof eventName != "string" ||
    typeof capture != "boolean" ||
    (checkFn && typeof checkFn != "function") ||
    (timeout !== null && typeof timeout != "number") ||
    typeof mozSystemGroup != "boolean" ||
    typeof wantUntrusted != "boolean"
  ) {
    throw new TypeError();
  }
  if (timeout < 0) {
    throw new RangeError();
  }

  return new Promise((resolve, reject) => {
    let timer;

    function cleanUp() {
      subject.removeEventListener(eventName, listener, capture);
      timer?.cancel();
    }

    function listener(event) {
      lazy.logger.trace(`Received DOM event ${event.type} for ${event.target}`);
      try {
        if (checkFn && !checkFn(event)) {
          return;
        }
      } catch (e) {
        // Treat an exception in the callback as a falsy value
        lazy.logger.warn(`Event check failed: ${e.message}`);
      }

      cleanUp();
      executeSoon(() => resolve(event));
    }

    subject.addEventListener(eventName, listener, {
      capture,
      mozSystemGroup,
      wantUntrusted,
    });

    if (timeout !== null) {
      timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      timer.init(
        () => {
          cleanUp();
          reject(
            new lazy.error.TimeoutError(
              `EventPromise timed out after ${timeout} ms`
            )
          );
        },
        timeout,
        TYPE_ONE_SHOT
      );
    }
  });
}

/**
 * Wait for the next tick in the event loop to execute a callback.
 *
 * @param {Function} fn
 *     Function to be executed.
 */
export function executeSoon(fn) {
  if (typeof fn != "function") {
    throw new TypeError();
  }

  Services.tm.dispatchToMainThread(fn);
}

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
 *     }, {timeout: 1000});
 *
 * @param {Condition} func
 *     Function to run off the main thread.
 * @param {object=} options
 * @param {string=} options.errorMessage
 *     Message to use to send a warning if ``timeout`` is over.
 *     Defaults to `PollPromise timed out`.
 * @param {number=} options.timeout
 *     Desired timeout if wanted.  If 0 or less than the runtime evaluation
 *     time of ``func``, ``func`` is guaranteed to run at least once.
 *     Defaults to using no timeout.
 * @param {number=} options.interval
 *     Duration between each poll of ``func`` in milliseconds.
 *     Defaults to 10 milliseconds.
 *
 * @returns {Promise.<*>}
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
export function PollPromise(func, options = {}) {
  const {
    errorMessage = "PollPromise timed out",
    interval = 10,
    timeout = null,
  } = options;
  const timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  let didTimeOut = false;

  if (typeof func != "function") {
    throw new TypeError();
  }
  if (timeout != null && typeof timeout != "number") {
    throw new TypeError();
  }
  if (typeof interval != "number") {
    throw new TypeError();
  }
  if (
    (timeout && (!Number.isInteger(timeout) || timeout < 0)) ||
    !Number.isInteger(interval) ||
    interval < 0
  ) {
    throw new RangeError();
  }

  return new Promise((resolve, reject) => {
    let start, end;

    if (Number.isInteger(timeout)) {
      start = new Date().getTime();
      end = start + timeout;
    }

    let evalFn = () => {
      new Promise(func)
        .then(resolve, rejected => {
          if (typeof rejected != "undefined") {
            throw rejected;
          }

          // return if there is a timeout and set to 0,
          // allowing |func| to be evaluated at least once
          if (
            typeof end != "undefined" &&
            (start == end || new Date().getTime() >= end)
          ) {
            didTimeOut = true;
            resolve(rejected);
          }
        })
        .catch(reject);
    };

    // the repeating slack timer waits |interval|
    // before invoking |evalFn|
    evalFn();

    timer.init(evalFn, interval, TYPE_REPEATING_SLACK);
  }).then(
    res => {
      if (didTimeOut) {
        lazy.logger.warn(`${errorMessage} after ${timeout} ms`);
      }
      timer.cancel();
      return res;
    },
    err => {
      timer.cancel();
      throw err;
    }
  );
}

/**
 * Represents the timed, eventual completion (or failure) of an
 * asynchronous operation, and its resulting value.
 *
 * In contrast to a regular Promise, it times out after ``timeout``.
 *
 * @param {Function} fn
 *     Function to run, which will have its ``reject``
 *     callback invoked after the ``timeout`` duration is reached.
 *     It is given two callbacks: ``resolve(value)`` and
 *     ``reject(error)``.
 * @param {object=} options
 * @param {string} options.errorMessage
 *     Message to use for the thrown error.
 * @param {number=} options.timeout
 *     ``condition``'s ``reject`` callback will be called
 *     after this timeout, given in milliseconds.
 *     By default 1500 ms in an optimised build and 4500 ms in
 *     debug builds.
 * @param {Error=} options.throws
 *     When the ``timeout`` is hit, this error class will be
 *     thrown.  If it is null, no error is thrown and the promise is
 *     instead resolved on timeout with a TimeoutError.
 *
 * @returns {Promise.<*>}
 *     Timed promise.
 *
 * @throws {TypeError}
 *     If `timeout` is not a number.
 * @throws {RangeError}
 *     If `timeout` is not an unsigned integer.
 */
export function TimedPromise(fn, options = {}) {
  const {
    errorMessage = "TimedPromise timed out",
    timeout = PROMISE_TIMEOUT,
    throws = lazy.error.TimeoutError,
  } = options;

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
    let trace;

    // Reject only if |throws| is given.  Otherwise it is assumed that
    // the user is OK with the promise timing out.
    let bail = () => {
      const message = `${errorMessage} after ${timeout} ms`;
      if (throws !== null) {
        let err = new throws(message);
        reject(err);
      } else {
        lazy.logger.warn(message, trace);
        resolve();
      }
    };

    trace = lazy.error.stack();
    timer.initWithCallback({ notify: bail }, timeout, TYPE_ONE_SHOT);

    try {
      fn(resolve, reject);
    } catch (e) {
      reject(e);
    }
  }).then(
    res => {
      timer.cancel();
      return res;
    },
    err => {
      timer.cancel();
      throw err;
    }
  );
}
