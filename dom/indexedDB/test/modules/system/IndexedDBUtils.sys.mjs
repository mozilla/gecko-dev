/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

export const IndexedDBUtils = {
  /**
   * Handles the completion of a request, awaiting either the `onsuccess` or
   * `onerror` event before proceeding.
   *
   * This function is designed to handle requests of the types:
   * - `IDBRequest`
   * - `IDBOpenDBRequest`
   *
   * These requests are typically returned by IndexedDB API.
   *
   * @param {IDBRequest|IDBOpenDBRequest} request
   *   The request object, which must have `onsuccess` and `onerror` event
   *   handlers, as well as result and error properties.
   * @returns {Promise}
   *   Resolves with the request's result when the operation is successful.
   * @throws {Error}
   *   If the request encounters an error, this function throws the request's
   *   `error` property.
   */
  async requestFinished(request) {
    await new Promise(function (resolve) {
      request.onerror = function () {
        resolve();
      };
      request.onsuccess = function () {
        resolve();
      };
    });

    if (request.error) {
      throw request.error;
    }

    return request.result;
  },
};
