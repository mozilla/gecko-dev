/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

typedef DOMString HistogramID;
typedef DOMString HistogramKey;

[ChromeOnly, Exposed=Window]
namespace TelemetryStopwatch {
  /**
   * Starts a timer associated with a telemetry histogram. The timer can be
   * directly associated with a histogram, or with a pair of a histogram and
   * an object.
   *
   * @param histogram - a string which must be a valid histogram name.
   *
   * @param obj - Optional parameter. If specified, the timer is
   *              associated with this object, meaning that multiple
   *              timers for the same histogram may be run
   *              concurrently, as long as they are associated with
   *              different objects.
   * @param [options.inSeconds=false] - record elapsed time for this
   *        histogram in seconds instead of milliseconds. Defaults to
   *        false.
   *
   * @returns True if the timer was successfully started, false
   *          otherwise. If a timer already exists, it can't be
   *          started again, and the existing one will be cleared in
   *          order to avoid measurements errors.
   */
  boolean start(HistogramID histogram, optional object? obj = null,
                optional TelemetryStopwatchOptions options = {});

  /**
   * Deletes the timer associated with a telemetry histogram. The timer can be
   * directly associated with a histogram, or with a pair of a histogram and
   * an object. Important: Only use this method when a legitimate cancellation
   * should be done.
   *
   * @param histogram - a string which must be a valid histogram name.
   *
   * @param obj - Optional parameter. If specified, the timer is
   *              associated with this object, meaning that multiple
   *              timers or a same histogram may be run concurrently,
   *              as long as they are associated with different
   *              objects.
   *
   * @returns True if the timer exist and it was cleared, False
   *          otherwise.
   */
  boolean cancel(HistogramID histogram, optional object? obj = null);

  /**
   * Stops the timer associated with the given histogram (and object),
   * calculates the time delta between start and finish, and adds the value
   * to the histogram.
   *
   * @param histogram - a string which must be a valid histogram name.
   *
   * @param obj - Optional parameter which associates the histogram
   *              timer with the given object.
   *
   * @param canceledOkay - Optional parameter which will suppress any
   *                       warnings that normally fire when a stopwatch
   *                       is finished after being cancelled. Defaults
   *                       to false.
   *
   * @returns True if the timer was succesfully stopped and the data
   *          was added to the histogram, false otherwise.
   */
  boolean finish(HistogramID histogram,
                 optional object? obj = null,
                 optional boolean canceledOkay = false);

  /**
   * Set the testing mode. Used by tests.
   */
  undefined setTestModeEnabled(optional boolean testing = true);
};

dictionary TelemetryStopwatchOptions {
  /**
   * If true, record elapsed time for this histogram in seconds instead of
   * milliseconds.
   */
  boolean inSeconds = false;
};
