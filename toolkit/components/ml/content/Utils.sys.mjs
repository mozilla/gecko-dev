/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/**
 * Enumeration for the progress status text.
 */
export const ProgressStatusText = Object.freeze({
  // The value of the status text indicating that an operation is started.
  INITIATE: "initiate",
  // The value of the status text indicating an estimate for the size of the operation.
  SIZE_ESTIMATE: "size_estimate",
  // The value of the status text indicating that an operation is in progress.
  IN_PROGRESS: "in_progress",
  // The value of the status text indicating that an operation has completed.
  DONE: "done",
});

/**
 * Enumeration for type of progress operations.
 */
export const ProgressType = Object.freeze({
  // The value of the operation type for a remote downloading.
  DOWNLOAD: "downloading",
  // The value of the operation type when loading from cache
  LOAD_FROM_CACHE: "loading_from_cache",
  // The value of the operation type when running the model
  INFERENCE: "running_inference",
});

/**
 * This class encapsulates the parameters supported by a progress and status callback.
 */
export class ProgressAndStatusCallbackParams {
  // Params for progress callback

  /**
   * A float indicating the percentage of data loaded. Note that
   * 100% does not necessarily mean the operation is complete.
   *
   * @type {?float}
   */
  progress = null;

  /**
   * A float indicating the total amount of data loaded so far.
   * In particular, this is the sum of currentLoaded across all call of the callback.
   *
   * @type {?float}
   */
  totalLoaded = null;

  /**
   * The amount of data loaded in the current callback call.
   *
   * @type {?float}
   */
  currentLoaded = null;

  /**
   * A float indicating an estimate of the total amount of data to be loaded.
   * Do not rely on this number as this is an estimate and the true total could be
   * either lower or higher.
   *
   * @type {?float}
   */
  total = null;

  /**
   * The units in which the amounts are reported.
   *
   * @type {?string}
   */
  units = null;

  // Params for status callback
  /**
   * The name of the operation being tracked.
   *
   * @type {?string}
   */
  type = null;

  /**
   * A message indicating the status of the tracked operation.
   *
   * @type {?string}
   */
  statusText = null;

  /**
   * An ID uniquely identifying the object/file being tracked.
   *
   * @type {?string}
   */
  id = null;

  /**
   * A boolean indicating if the operation was successful.
   * true means we have a successful operation.
   *
   * @type {?boolean}
   */
  ok = null;

  /**
   * Any additional metadata for the operation being tracked.
   *
   * @type {?object}
   */
  metadata = null;

  constructor(params = {}) {
    this.update(params);
  }

  update(params = {}) {
    const allowedKeys = new Set(Object.keys(this));
    const invalidKeys = Object.keys(params).filter(x => !allowedKeys.has(x));
    if (invalidKeys.length) {
      throw new Error(`Received Invalid option: ${invalidKeys}`);
    }
    for (const key of allowedKeys) {
      if (key in params) {
        this[key] = params[key];
      }
    }
  }
}

/**
 * Read and track progress when reading a Response object
 *
 * @param {any} response The Response object to read
 * @param {?function(ProgressAndStatusCallbackParams):void} progressCallback The function to call with progress updates
 *
 * @returns {Promise<Uint8Array>} A Promise that resolves with the Uint8Array buffer
 */
export async function readResponse(response, progressCallback) {
  const contentLength = response.headers.get("Content-Length");
  if (!contentLength) {
    console.warn(
      "Unable to determine content-length from response headers. Will expand buffer when needed."
    );
  }
  let total = parseInt(contentLength ?? "0");

  progressCallback?.(
    new ProgressAndStatusCallbackParams({
      progress: 0,
      totalLoaded: 0,
      currentLoaded: 0,
      total,
      units: "bytes",
    })
  );

  let buffer = new Uint8Array(total);
  let loaded = 0;

  for await (const value of response.body) {
    let newLoaded = loaded + value.length;
    if (newLoaded > total) {
      total = newLoaded;

      // Adding the new data will overflow buffer.
      // In this case, we extend the buffer
      // Happened when the content-length is lower than the actual lenght
      let newBuffer = new Uint8Array(total);

      // copy contents
      newBuffer.set(buffer);

      buffer = newBuffer;
    }
    buffer.set(value, loaded);
    loaded = newLoaded;

    const progress = (loaded / total) * 100;

    progressCallback?.(
      new ProgressAndStatusCallbackParams({
        progress,
        totalLoaded: loaded,
        currentLoaded: value.length,
        total,
        units: "bytes",
      })
    );
  }

  // Ensure that buffer is not bigger than loaded
  // Sometimes content length is larger than the actual size
  buffer = buffer.slice(0, loaded);

  return buffer;
}

/**
 * Class for watching the progress bar of multiple events and combining
 * then into a single progress bar.
 */
export class MultiProgressAggregator {
  /**
   * A function to call with the aggregated statistics.
   *
   * @type {?function(ProgressAndStatusCallbackParams):void}
   */
  progressCallback = null;

  /**
   * The name of the key that contains status information.
   *
   * @type {Set<string>}
   */
  watchedTypes;

  /**
   * The total amount of information loaded so far.
   *
   * @type {float}
   */
  #combinedLoaded = 0;

  /**
   * The total amount of information to be loaded.
   *
   * @type {float}
   */
  #combinedTotal = 0;

  /**
   * The number of operations that are yet to be completed.
   *
   * @type {float}
   */
  #remainingEvents = 0;

  /**
   * The type of operation seen so far.
   *
   * @type {Set<string>}
   */
  #seenTypes;

  /**
   * The status of text seen so far.
   *
   * @type {Set<string>}
   */
  #seenStatus;

  /**
   * @param {object} config
   * @param {?function(ProgressAndStatusCallbackParams):void} config.progressCallback - A function to call with the aggregated statistics.
   * @param {Iterable<string>} config.watchedTypes - The types to watch for aggregation
   */
  constructor({ progressCallback, watchedTypes = [ProgressType.DOWNLOAD] }) {
    this.progressCallback = progressCallback;
    this.watchedTypes = new Set(watchedTypes);

    this.#seenTypes = new Set();
    this.#seenStatus = new Set();
  }

  /**
   * Callback function that will combined data from different objects/files.
   *
   * @param {ProgressAndStatusCallbackParams} data - object containing the data
   */
  aggregateCallback(data) {
    if (this.watchedTypes.has(data.type)) {
      this.#seenTypes.add(data.type);
      this.#seenStatus.add(data.statusText);
      if (data.statusText == ProgressStatusText.INITIATE) {
        this.#remainingEvents += 1;
      }

      if (data.statusText == ProgressStatusText.SIZE_ESTIMATE) {
        this.#combinedTotal += data.total ?? 0;
      }

      if (data.statusText == ProgressStatusText.DONE) {
        this.#remainingEvents -= 1;
      }

      this.#combinedLoaded += data.currentLoaded ?? 0;

      if (this.progressCallback) {
        let statusText = data.statusText;
        if (this.#seenStatus.has(ProgressStatusText.IN_PROGRESS)) {
          statusText = ProgressStatusText.IN_PROGRESS;
        }

        if (this.#remainingEvents == 0) {
          statusText = ProgressStatusText.DONE;
        }

        this.progressCallback(
          new ProgressAndStatusCallbackParams({
            type: data.type,
            statusText,
            id: data.id,
            total: this.#combinedTotal,
            currentLoaded: data.currentLoaded,
            totalLoaded: this.#combinedLoaded,
            progress: (this.#combinedLoaded / this.#combinedTotal) * 100,
            ok: data.ok,
            units: data.units,
            metadata: data,
          })
        );
      }
    }
  }
}

/**
 * Converts a model and its headers to a Response object.
 *
 * @param {ArrayBuffer} modelFile
 * @param {object|null} headers
 * @returns {Response} The generated Response instance
 */
export function modelToResponse(modelFile, headers) {
  let responseHeaders = {};

  if (headers) {
    // Headers are converted to strings, as the cache may hold int keys like fileSize
    for (let key in headers) {
      if (headers[key] != null) {
        responseHeaders[key] = headers[key].toString();
      }
    }
  }

  return new Response(modelFile, {
    status: 200,
    headers: responseHeaders,
  });
}

// Create a "namespace" to make it easier to import multiple names.
export var Progress = Progress || {};
Progress.ProgressAndStatusCallbackParams = ProgressAndStatusCallbackParams;
Progress.ProgressStatusText = ProgressStatusText;
Progress.ProgressType = ProgressType;
Progress.readResponse = readResponse;

export async function getInferenceProcessInfo() {
  // for now we only have a single inference process.
  let info = await ChromeUtils.requestProcInfo();

  for (const child of info.children) {
    if (child.type === "inference") {
      return {
        pid: child.pid,
        memory: child.memory,
        cpuTime: child.cpuTime,
        cpuCycleCount: child.cpuCycleCount,
      };
    }
  }
  return {};
}

const ALWAYS_ALLOWED_HUBS = [
  "chrome://",
  "resource://",
  "http://localhost/",
  "https://localhost/",
];

/**
 * Enum for URL rejection types.
 *
 * Defines the type of rejection for a URL:
 *
 * - "DENIED" is for URLs explicitly disallowed by the deny list.
 * - "NONE" is for URLs allowed by the allow list.
 * - "DISALLOWED" is for URLs not matching any entry in either list.
 *
 * @readonly
 * @enum {string}
 */
export const RejectionType = {
  DENIED: "DENIED",
  NONE: "NONE",
  DISALLOWED: "DISALLOWED",
};

/**
 * Class for checking URLs against allow and deny lists.
 */
export class URLChecker {
  /**
   * Creates an instance of URLChecker.
   *
   * @param {Array<{filter: 'ALLOW'|'DENY', urlPrefix: string}>} allowDenyList - Array of URL patterns with filters.
   */
  constructor(allowDenyList = null) {
    if (allowDenyList) {
      this.allowList = allowDenyList
        .filter(entry => entry.filter === "ALLOW")
        .map(entry => entry.urlPrefix.toLowerCase());

      this.denyList = allowDenyList
        .filter(entry => entry.filter === "DENY")
        .map(entry => entry.urlPrefix.toLowerCase());
    } else {
      this.allowList = [];
      this.denyList = [];
    }

    // Always allowed
    for (const url of ALWAYS_ALLOWED_HUBS) {
      this.allowList.push(url);
    }
  }

  /**
   * Normalizes localhost URLs to ignore user info, port, and path details.
   *
   * @param {string} url - The URL to normalize.
   * @returns {string} - Normalized URL.
   */
  normalizeLocalhost(url) {
    try {
      const parsedURL = new URL(url);
      if (parsedURL.hostname === "localhost") {
        // Normalize to only scheme and localhost without port or user info
        return `${parsedURL.protocol}//localhost/`;
      }
      return url;
    } catch (error) {
      return url;
    }
  }

  /**
   * Checks if a given URL is allowed based on allowList and denyList patterns.
   *
   * @param {string} url - The URL to check.
   * @returns {{ allowed: boolean, rejectionType: string }} - Returns an object with:
   *    - `allowed`: true if the URL is allowed, otherwise false.
   *    - `rejectionType`:
   *       - "DENIED" if the URL matches an entry in the denyList,
   *       - "NONE" if the URL matches an entry in the allowList,
   *       - "DISALLOWED" if the URL does not match any entry in either list.
   */
  allowedURL(url) {
    const normalizedURL = this.normalizeLocalhost(url).toLowerCase();

    // Check if the URL is denied by any entry in the denyList
    if (this.denyList.some(prefix => normalizedURL.startsWith(prefix))) {
      return { allowed: false, rejectionType: RejectionType.DENIED };
    }

    // Check if the URL is allowed by any entry in the allowList
    if (this.allowList.some(prefix => normalizedURL.startsWith(prefix))) {
      return { allowed: true, rejectionType: RejectionType.NONE };
    }

    // If no matches, return a default rejectionType
    return { allowed: false, rejectionType: RejectionType.DISALLOWED };
  }
}
