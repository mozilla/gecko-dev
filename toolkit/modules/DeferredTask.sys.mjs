/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Timer = Components.Constructor(
  "@mozilla.org/timer;1",
  "nsITimer",
  "initWithCallback"
);

/**
 * Sets up a function or an asynchronous task whose execution can be triggered
 * after a defined delay.  Multiple attempts to run the task before the delay
 * has passed are coalesced.  The task cannot be re-entered while running, but
 * can be executed again after a previous run finished.
 *
 * A common use case occurs when a data structure should be saved into a file
 * every time the data changes, using asynchronous calls, and multiple changes
 * to the data may happen within a short time:
 *
 *   let saveDeferredTask = new DeferredTask(async function() {
 *     await OS.File.writeAtomic(...);
 *     // Any uncaught exception will be reported.
 *   }, 2000);
 *
 *   // The task is ready, but will not be executed until requested.
 *
 * The "arm" method can be used to start the internal timer that will result in
 * the eventual execution of the task.  Multiple attempts to arm the timer don't
 * introduce further delays:
 *
 *   saveDeferredTask.arm();
 *
 *   // The task will be executed in 2 seconds from now.
 *
 *   await waitOneSecond();
 *   saveDeferredTask.arm();
 *
 *   // The task will be executed in 1 second from now.
 *
 * The timer can be disarmed to reset the delay, or just to cancel execution:
 *
 *   saveDeferredTask.disarm();
 *   saveDeferredTask.arm();
 *
 *   // The task will be executed in 2 seconds from now.
 *
 * When the internal timer fires and the execution of the task starts, the task
 * cannot be canceled anymore.  It is however possible to arm the timer again
 * during the execution of the task, in which case the task will need to finish
 * before the timer is started again, thus guaranteeing a time of inactivity
 * between executions that is at least equal to the provided delay.
 *
 * The "finalize" method can be used to ensure that the task terminates
 * properly.  The promise it returns is resolved only after the last execution
 * of the task is finished.  To guarantee that the task is executed for the
 * last time, the method prevents any attempt to arm the timer again.
 *
 * If the timer is already armed when the "finalize" method is called, then the
 * task is executed immediately.  If the task was already running at this point,
 * then one last execution from start to finish will happen again, immediately
 * after the current execution terminates.  If the timer is not armed, the
 * "finalize" method only ensures that any running task terminates.
 *
 * For example, during shutdown, you may want to ensure that any pending write
 * is processed, using the latest version of the data if the timer is armed:
 *
 *   AsyncShutdown.profileBeforeChange.addBlocker(
 *     "Example service: shutting down",
 *     () => saveDeferredTask.finalize()
 *   );
 *
 * Instead, if you are going to delete the saved data from disk anyways, you
 * might as well prevent any pending write from starting, while still ensuring
 * that any write that is currently in progress terminates, so that the file is
 * not in use anymore:
 *
 *   saveDeferredTask.disarm();
 *   saveDeferredTask.finalize().then(() => OS.File.remove(...))
 *                              .then(null, Components.utils.reportError);
 */
export class DeferredTask {
  /**
   * Sets up a task whose execution can be triggered after a delay.
   *
   * @param {Function} taskFn
   *   Function to execute.  If the function returns a promise, the task is not
   *   considered complete until that promise resolves. This task is never
   *   re-entered while running.
   * @param {number} delayMs
   *   Time between executions, in milliseconds.  Multiple attempts to run the
   *   task before the delay has passed are coalesced.  This time of inactivity
   *   is guaranteed to pass between multiple executions of the task, except on
   *   finalization, when the task may restart immediately after the previous
   *   execution finished.
   * @param {number} [idleTimeoutMs]
   *        The maximum time to wait for an idle slot on the main thread after
   *        aDelayMs have elapsed. If omitted, waits indefinitely for an idle
   *        callback.
   */
  constructor(taskFn, delayMs, idleTimeoutMs) {
    this.#taskFn = taskFn;
    this.#delayMs = delayMs;
    this._idleTimeoutMs = idleTimeoutMs;
    this.#caller = new Error().stack.split("\n", 2)[1];
    let markerString = `delay: ${delayMs}ms`;
    if (idleTimeoutMs) {
      markerString += `, idle timeout: ${idleTimeoutMs}`;
    }
    ChromeUtils.addProfilerMarker(
      "DeferredTask",
      { captureStack: true },
      markerString
    );
  }

  /**
   * Function to execute.
   */
  #taskFn;

  /**
   * Time between executions, in milliseconds.
   */
  #delayMs;

  /**
   * The idle timeout wait. Exposed for tests.
   *
   * @type {number|undefined}
   */
  _idleTimeoutMs = undefined;

  /**
   * The name of the caller that created the deferred task.
   */
  #caller;

  /**
   * Indicates whether the task is currently requested to start again later,
   * regardless of whether it is currently running.
   */
  get isArmed() {
    return this.#armed;
  }
  #armed = false;

  /**
   * Indicates whether the task is currently running.  This is always true when
   * read from code inside the task function, but can also be true when read
   * from external code, in case the task is an asynchronous function.
   */
  get isRunning() {
    return !!this._runningPromise;
  }

  /**
   * Promise resolved when the current execution of the task terminates, or null
   * if the task is not currently running.
   *
   * May be accessed for tests.
   *
   * @type {Promise<void>|undefined}
   */
  _runningPromise = undefined;

  /**
   * nsITimer used for triggering the task after a delay, or null in case the
   * task is running or there is no task scheduled for execution.
   *
   * @type {nsITimer|null}
   */
  #timer = null;

  /**
   * Actually starts the timer with the delay specified on construction.
   */
  #startTimer() {
    let callback, timer;
    if (this._idleTimeoutMs === 0) {
      callback = () => this.#timerCallback();
    } else {
      callback = () => {
        this._startIdleDispatch(() => {
          // #timer could have changed by now:
          // - to null if disarm() or finalize() has been called.
          // - to a new nsITimer if disarm() was called, followed by arm().
          // In either case, don't invoke #timerCallback any more.
          if (this.#timer === timer) {
            this.#timerCallback();
          }
        }, this._idleTimeoutMs);
      };
    }
    timer = new Timer(callback, this.#delayMs, Ci.nsITimer.TYPE_ONE_SHOT);
    this.#timer = timer;
  }

  /**
   * Dispatches idle task. Can be overridden for testing by test_DeferredTask.
   *
   * @param {IdleRequestCallback} callback
   * @param {number} timeout
   */
  _startIdleDispatch(callback, timeout) {
    ChromeUtils.idleDispatch(callback, { timeout });
  }

  /**
   * Requests the execution of the task after the delay specified on
   * construction.  Multiple calls don't introduce further delays.  If the task
   * is running, the delay will start when the current execution finishes.
   *
   * The task will always be executed on a different tick of the event loop,
   * even if the delay specified on construction is zero.  Multiple "arm" calls
   * within the same tick of the event loop are guaranteed to result in a single
   * execution of the task.
   *
   * Note: By design, this method doesn't provide a way for the caller to detect
   *       when the next execution terminates, or collect a result.  In fact,
   *       doing that would often result in duplicate processing or logging.  If
   *       a special operation or error logging is needed on completion, it can
   *       be better handled from within the task itself, for example using a
   *       try/catch/finally clause in the task.  The "finalize" method can be
   *       used in the common case of waiting for completion on shutdown.
   */
  arm() {
    if (this.#finalized) {
      throw new Error("Unable to arm timer, the object has been finalized.");
    }

    this.#armed = true;

    // In case the timer callback is running, do not create the timer now,
    // because this will be handled by the timer callback itself.  Also, the
    // timer is not restarted in case it is already running.
    if (!this._runningPromise && !this.#timer) {
      this.#startTimer();
    }
  }

  /**
   * Cancels any request for a delayed the execution of the task, though the
   * task itself cannot be canceled in case it is already running.
   *
   * This method stops any currently running timer, thus the delay will restart
   * from its original value in case the "arm" method is called again.
   */
  disarm() {
    this.#armed = false;
    if (this.#timer) {
      // Calling the "cancel" method and discarding the timer reference makes
      // sure that the timer callback will not be called later, even if the
      // timer thread has already posted the timer event on the main thread.
      this.#timer.cancel();
      this.#timer = null;
    }
  }

  /**
   * Ensures that any pending task is executed from start to finish, while
   * preventing any attempt to arm the timer again.
   *
   * - If the task is running and the timer is armed, then one last execution
   *   from start to finish will happen again, immediately after the current
   *   execution terminates, then the returned promise will be resolved.
   * - If the task is running and the timer is not armed, the returned promise
   *   will be resolved when the current execution terminates.
   * - If the task is not running and the timer is armed, then the task is
   *   started immediately, and the returned promise resolves when the new
   *   execution terminates.
   * - If the task is not running and the timer is not armed, the method returns
   *   a resolved promise.
   *
   * @returns {Promise<void>}
   *   Resolves when the last execution of the task is finished.
   */
  finalize() {
    if (this.#finalized) {
      throw new Error("The object has been already finalized.");
    }
    this.#finalized = true;

    // If the timer is armed, it means that the task is not running but it is
    // scheduled for execution.  Cancel the timer and run the task immediately,
    // so we don't risk blocking async shutdown longer than necessary.
    if (this.#timer) {
      this.disarm();
      this.#timerCallback();
    }

    // Wait for the operation to be completed, or resolve immediately.
    if (this._runningPromise) {
      return this._runningPromise;
    }
    return Promise.resolve();
  }
  #finalized = false;

  /**
   * Whether the DeferredTask has been finalized, and it cannot be armed anymore.
   */
  get isFinalized() {
    return this.#finalized;
  }

  /**
   * Timer callback used to run the delayed task.
   */
  #timerCallback() {
    let runningDeferred = Promise.withResolvers();

    // All these state changes must occur at the same time directly inside the
    // timer callback, to prevent race conditions and to ensure that all the
    // methods behave consistently even if called from inside the task.  This
    // means that the assignment of "this._runningPromise" must complete before
    // the task gets a chance to start.
    this.#timer = null;
    this.#armed = false;
    this._runningPromise = runningDeferred.promise;

    runningDeferred.resolve(
      (async () => {
        // Execute the provided function asynchronously.
        await this.#runTask();

        // Now that the task has finished, we check the state of the object to
        // determine if we should restart the task again.
        if (this.#armed) {
          if (!this.#finalized) {
            this.#startTimer();
          } else {
            // Execute the task again immediately, for the last time.  The isArmed
            // property should return false while the task is running, and should
            // remain false after the last execution terminates.
            this.#armed = false;
            await this.#runTask();
          }
        }

        // Indicate that the execution of the task has finished.  This happens
        // synchronously with the previous state changes in the function.
        this._runningPromise = null;
      })().catch(console.error)
    );
  }

  /**
   * Executes the associated task and catches exceptions.
   */
  async #runTask() {
    let startTime = Cu.now();
    try {
      await this.#taskFn();
    } catch (ex) {
      console.error(ex);
    } finally {
      ChromeUtils.addProfilerMarker(
        "DeferredTask",
        { startTime },
        this.#caller
      );
    }
  }
}
