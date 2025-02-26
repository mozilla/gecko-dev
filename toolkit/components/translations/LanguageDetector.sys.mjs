/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { clearTimeout, setTimeout } from "resource://gre/modules/Timer.sys.mjs";

const WORKER_URL = "resource://gre/modules/translations/cld-worker.js";

/**
 * The options used for when detecting a language.
 *
 * @typedef {object} DetectionOptions
 *
 * @property {string} text - The text to analyze.
 * @property {boolean} [isHTML] - A boolean, indicating whether the text should be analyzed as
 *     HTML rather than plain text.
 * @property {string} [language] - A string indicating the expected language. For text
 *     extracted from HTTP documents, this is expected to come from the Content-Language
 *     header.
 * @property {string} [tld] - A string indicating the top-level domain of the document the
 *     text was extracted from.
 * @property {string} [encoding] - A string describing the encoding of the document the
 *     string was extracted from. Note that, regardless of the value of this property,
 *     the 'text' property must be a UTF-16 JavaScript string.
 */

/**
 * A larger web document can be composed of multiple languages. This object details the
 * breakdown of what languages are present in the document, and at what percentages.
 * For instance a document could be 70% English and 30% French:
 *
 *   [
 *      { language: "en", percentage: 70 },
 *      { language: "fr", percentage: 30 },
 *   ]
 *
 * @typedef {object} MultilingualSection
 * @property {string} language - BCP 47 language tag, or "un" for unknown.
 * @property {number} percent - The integral percentage ranged 0-100.
 */

/**
 * @typedef {object} DetectionResult
 * @property {string} language - The language code
 * @property {boolean} confident - Whether the detector is confident of the result.
 * @property {Array<MultilingualSection>} languages - The list of languages detected in
 *     multilingual content. This is between 0 and 3 languages.
 */

/**
 * The length of the substring to pull from the document's text for language
 * identification.
 *
 * This value should ideally be one that is large enough to yield a confident
 * identification result without being too large or expensive to extract.
 *
 * At this time, this value is not driven by statistical data or analysis.
 */
const DOC_TEXT_TO_IDENTIFY_LENGTH = 1024;

/**
 * The shorter the text, the less confidence we should have in the result of the language
 * identification. Add another heuristic to report the ID as not confident if the length
 * of the code points of the text is less than this threshold.
 *
 * This was determined by plotting a kernel density estimation of the number of times the
 * source language had to be changed in the SelectTranslationsPanel vs. the code units in
 * the source text.
 *
 * 0013 code units or less - 49.5% of language changes
 * 0036 code units or less - 74.9% of language changes
 * 0153 code units or less - 90.0% of language changes
 * 0200 code units or less - 91.5% of language changes
 * 0427 code units or less - 95.0% of language changes
 * 1382 code units or less - 98.0% of language changes
 * 3506 code units or less - 99.0% of language changes
 */
const DOC_CONFIDENCE_THRESHOLD = 200;

/**
 * An internal class to manage communicating to the worker, and managing its lifecycle.
 * It's initialized once below statically to the module.
 */
class WorkerManager {
  // Since Emscripten can handle heap growth, but not heap shrinkage, we need to refresh
  // the worker after we've processed a particularly large string in order to prevent
  // unnecessary resident memory growth.
  //
  // These values define the cut-off string length and the idle timeout (in milliseconds)
  // before destroying a worker. Once a string of the maximum size has been processed,
  // the worker is marked for destruction, and is terminated as soon as it has been idle
  // for the given timeout.
  //
  // 1.5MB. This is the approximate string length that forces heap growth for a 2MB heap.
  LARGE_STRING = 1.5 * 1024 * 1024;
  IDLE_TIMEOUT = 10_000;

  /**
   * Resolvers for the detection queue.
   *
   * @type {Array<(result: DetectionResult) => void>}
   */
  detectionQueue = [];

  /**
   * @type {Worker | null}
   */
  worker = null;

  /**
   * @type {Promise<Worker> | null}
   */
  workerPromise = null;

  /**
   * Holds the ID of the current pending idle cleanup setTimeout.
   *
   * @type {number | null}
   */
  idleTimeoutId = null;

  /**
   * @param {DetectionOptions} options
   * @returns {Promise<DetectionResult>}
   */
  async detectLanguage(options) {
    const worker = await this.getWorker();

    const result = await new Promise(resolve => {
      this.detectionQueue.push(resolve);
      worker.postMessage(options);
    });

    // We have our asynchronous result from the worker.
    //
    // Determine if our input was large enough to trigger heap growth,
    // or if we're already waiting to destroy the worker when it's
    // idle. If so, schedule termination after the idle timeout.
    if (
      options.text.length >= this.LARGE_STRING ||
      this.idleTimeoutId != null
    ) {
      this.flushWorker();
    }

    return result;
  }

  /**
   * @returns {Promise<Worker>}
   */
  getWorker() {
    if (!this.workerPromise) {
      this.workerPromise = new Promise(resolve => {
        let worker = new Worker(WORKER_URL);
        worker.onmessage = message => {
          if (message.data == "ready") {
            resolve(worker);
          } else {
            /** @type {DetectionResult} */
            const detectionResult = message.data;

            const resolver = this.detectionQueue.shift();
            resolver(detectionResult);
          }
        };
        this.worker = worker;
      });
    }

    return this.workerPromise;
  }

  /**
   * Schedule the current worker to be terminated after the idle timeout.
   */
  flushWorker() {
    if (this.idleTimeoutId != null) {
      clearTimeout(this.idleTimeoutId);
    }

    this.idleTimeoutId = setTimeout(() => {
      if (this.detectionQueue.length) {
        // Reschedule the termination as something else was added to the queue.
        this.flushWorker();
      } else {
        // Terminate the worker.
        if (this.worker) {
          this.worker.terminate();
        }

        this.worker = null;
        this.workerPromise = null;
        this.idleTimeoutId = null;
      }
    }, this.IDLE_TIMEOUT);
  }
}

/**
 * The worker manager is static to this module. Exported it for unit testing.
 */
export const workerManager = new WorkerManager();

/**
 *
 */
export class LanguageDetector {
  /**
   * Detect the language of a given string.
   *
   * @param {DetectionOptions | string} options - Either the text to analyze,
   *     or the options.
   * @returns {Promise<DetectionResult>}
   */
  static detectLanguage(options) {
    if (typeof options == "string") {
      options = { text: options };
    }

    return workerManager.detectLanguage(options);
  }

  /**
   * Attempts to determine the language in which the document's content is written.
   *
   * @param {Document} document
   * @returns {DetectionResult}
   */
  static async detectLanguageFromDocument(document) {
    // Grab a selection of text.
    let encoder = Cu.createDocumentEncoder("text/plain");
    encoder.init(document, "text/plain", encoder.SkipInvisibleContent);
    let text = encoder
      .encodeToStringWithMaxLength(DOC_TEXT_TO_IDENTIFY_LENGTH)
      .replaceAll("\r", "")
      .replaceAll("\n", " ");

    const result = await workerManager.detectLanguage({
      text,
    });

    if (text.length < DOC_CONFIDENCE_THRESHOLD) {
      result.confident = false;
    }
    return result;
  }
}
