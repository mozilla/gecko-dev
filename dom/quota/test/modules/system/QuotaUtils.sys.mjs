/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

import { PrefUtils } from "resource://testing-common/dom/quota/test/modules/PrefUtils.sys.mjs";
import { RequestError } from "resource://testing-common/dom/quota/test/modules/RequestError.sys.mjs";

export const QuotaUtils = {
  /**
   * Handles the completion of a request, awaiting the callback to be called
   * before proceeding.
   *
   * This function is designed to handle requests of the types:
   * - `nsIQuotaRequest`
   * - `nsIQuotaUsageRequest`
   *
   * These requests are typically returned by the quota manager service.
   *
   * @param {Object} request
   *   The request object, which must have a callback property and
   *   result-related properties (e.g., resultCode, resultName).
   * @returns {Promise}
   *   Resolves with the request's result when the operation is successful.
   * @throws {RequestError}
   *   If the request's resultCode is not `Cr.NS_OK`, indicating an error in
   *   the request.
   */
  async requestFinished(request) {
    await new Promise(function (resolve) {
      request.callback = function () {
        resolve();
      };
    });

    if (request.resultCode !== Cr.NS_OK) {
      throw new RequestError(request.resultCode, request.resultName);
    }

    return request.result;
  },

  /**
   * Temporarily sets artificial failure preferences for testing, runs the
   * callback, and then restores the original preferences.
   *
   * @param {number} categories - A bitwise combination of artificial failure
   *   categories to set.
   * @param {number} probability - The probability (0-100) of triggering an
   *   artificial failure. A value of 0 means no failure, while 100 means
   *   failure is guaranteed.
   * @param {number} errorCode - The error code to return when an artificial
   *   failure occurs.
   * @param {Function} callback - The asynchronous function to execute with the
   *   artificial settings.
   * @returns {*} - The result of the callback function after it has been
   *   awaited.
   */
  async withArtificialFailures(categories, probability, errorCode, callback) {
    const prefs = [
      ["dom.quotaManager.artificialFailure.categories", categories],
      ["dom.quotaManager.artificialFailure.probability", probability],
      ["dom.quotaManager.artificialFailure.errorCode", errorCode],
    ];

    const originalPrefs = PrefUtils.getPrefs(prefs);

    let result = null;

    try {
      PrefUtils.setPrefs(prefs);

      result = await callback();
    } finally {
      PrefUtils.setPrefs(originalPrefs);
    }

    return result;
  },

  /**
   * Simulates an orderly shutdown sequence by advancing through predefined
   * application shutdown phases. This function mirrors the shutdown sequence
   * triggered during the xpcshell test cleanup phase:
   * https://searchfox.org/mozilla-central/rev/a2abcf7ff6b7ae0c2d8a04b9a35679f8c84634e7/testing/xpcshell/head.js#701-718
   *
   * Tests can use this function to prematurely trigger shutdown in order to
   * test edge cases related to events occurring during Quota Manager shutdown.
   */
  startShutdown() {
    const phases = [
      Services.startup.SHUTDOWN_PHASE_APPSHUTDOWNNETTEARDOWN,
      Services.startup.SHUTDOWN_PHASE_APPSHUTDOWNTEARDOWN,
      Services.startup.SHUTDOWN_PHASE_APPSHUTDOWN,
      Services.startup.SHUTDOWN_PHASE_APPSHUTDOWNQM,
    ];

    for (const phase of phases) {
      Services.startup.advanceShutdownPhase(phase);
    }
  },
};
