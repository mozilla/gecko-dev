/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const BYTES_IN_KILOBYTE = 1000;
export const BYTES_IN_MEGABYTE = 1000000;

export const BYTES_IN_KIBIBYTE = 1024;
export const BYTES_IN_MEBIBYTE = 1048576;

export const MeasurementUtils = {
  /**
   * Rounds byte sizes of user files in order to provide protection
   * against fingerprinting.
   *
   * @param {number} bytes
   *   Number of bytes to fuzz
   * @param {number} nearest
   *   Nearest number of bytes to round to
   * @returns {number}
   *   Number of bytes rounded to the `nearest` granularity
   *
   * @example fuzzByteSize(1500, 1000) === 2000
   * @example fuzzByteSize(1001, 1000) === 1000
   * @example fuzzByteSize(1024, 10) === 1020
   * @example fuzzByteSize(512, 1000) === 1000
   * @example fuzzByteSize(256, 1000) === 1000
   */
  fuzzByteSize(bytes, nearest) {
    const fuzzed = Math.round(bytes / nearest) * nearest;
    return Math.max(fuzzed, nearest);
  },

  /**
   * Helper wrapper for timing an async operation with Glean. This style of
   * helper is available in some Glean SDKs, but not JavaScript yet.
   *
   * @see https://mozilla.github.io/glean/book/reference/metrics/timing_distribution.html#measure
   *
   * @template T
   * @param {GleanTimingDistribution} timer
   *   Glean TimingDistribution object
   * @param {Promise<T>} operation
   *   Async operation to time
   * @returns {Promise<T>}
   *   Result of `operation`
   */
  async measure(timer, operation) {
    const timerId = timer.start();

    try {
      const result = await operation;
      timer.stopAndAccumulate(timerId);
      return result;
    } catch (err) {
      timer.cancel(timerId);
      throw err;
    }
  },
};
