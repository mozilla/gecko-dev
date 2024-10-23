/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/**
 * MLSuggest helps with ML based suggestions around intents and location.
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  createEngine: "chrome://global/content/ml/EngineProcess.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

/**
 * These INTENT_OPTIONS and NER_OPTIONS will go to remote setting server and depends
 * on https://bugzilla.mozilla.org/show_bug.cgi?id=1923553
 */
const INTENT_OPTIONS = {
  taskName: "text-classification",
  modelId: "mozilla/mobilebert-uncased-finetuned-LoRA-intent-classifier",
  modelRevision: "v0.1.0",
  dtype: "q8",
};

const NER_OPTIONS = {
  taskName: "token-classification",
  modelId: "mozilla/distilbert-uncased-NER-LoRA",
  modelRevision: "v0.1.1",
  dtype: "q8",
};

// List of prepositions used in subject cleaning.
const PREPOSITIONS = ["in", "at", "on", "for", "to", "near"];

/**
 * Class for handling ML-based suggestions using intent and NER models.
 *
 * @class
 */
class _MLSuggest {
  #modelEngines = {};

  /**
   * Initializes the intent and NER models.
   */
  async initialize() {
    await Promise.all([
      this.#initializeModelEngine(INTENT_OPTIONS),
      this.#initializeModelEngine(NER_OPTIONS),
    ]);
  }

  /**
   * Generates ML-based suggestions by finding intent, detecting entities, and
   * combining locations.
   *
   * @param {string} query
   *   The user's input query.
   * @returns {object | null}
   *   The suggestion result including intent, location, and subject, or null if
   *   an error occurs.
   *   {string} intent
   *     The predicted intent label of the query.
   *   - {object|null} location: The detected location from the query, which is
   *     an object with `city` and `state` fields:
   *     - {string|null} city: The detected city, or `null` if no city is found.
   *     - {string|null} state: The detected state, or `null` if no state is found.
   *   {string} subject
   *     The subject of the query after location is removed.
   *   {object} metrics
   *     The combined metrics from NER model results, representing additional
   *     information about the model's performance.
   */
  async makeSuggestions(query) {
    let intentRes, nerResult;
    try {
      [intentRes, nerResult] = await Promise.all([
        this._findIntent(query),
        this._findNER(query),
      ]);
    } catch (error) {
      return null;
    }

    if (!intentRes || !nerResult) {
      return null;
    }

    const locationResVal = await this.#combineLocations(
      nerResult,
      lazy.UrlbarPrefs.get("nerThreshold")
    );

    return {
      intent: intentRes,
      location: locationResVal,
      subject: this.#findSubjectFromQuery(query, locationResVal),
      metrics: this.#sumObjectsByKey(intentRes.metrics, nerResult.metrics),
    };
  }

  /**
   * Shuts down all initialized engines.
   */
  async shutdown() {
    for (const [key, engine] of Object.entries(this.#modelEngines)) {
      try {
        await engine.terminate?.();
      } finally {
        // Remove each engine after termination
        delete this.#modelEngines[key];
      }
    }
  }

  /**
   * Helper method to generate a unique key for model engines.
   *
   * @param {object} options
   *   The options object containing taskName and modelId.
   * @returns {string}
   *   The key for the model engine.
   */
  #getmodelEnginesKey(options) {
    return `${options.taskName}-${options.modelId}`;
  }

  async #initializeModelEngine(options) {
    const engineId = this.#getmodelEnginesKey(options);

    // uses cache if engine was used
    if (this.#modelEngines[engineId]) {
      return this.#modelEngines[engineId];
    }

    const engine = await lazy.createEngine({ ...options, engineId });
    // Cache the engine
    this.#modelEngines[engineId] = engine;
    return engine;
  }

  /**
   * Finds the intent of the query using the intent classification model.
   * (This has been made public to enable testing)
   *
   * @param {string} query
   *   The user's input query.
   * @param {object} options
   *   The options for the engine pipeline
   * @returns {string|null}
   *   The predicted intent label or null if the model is not initialized.
   */
  async _findIntent(query, options = {}) {
    const engineIntentClassifier =
      this.#modelEngines[this.#getmodelEnginesKey(INTENT_OPTIONS)];

    if (!engineIntentClassifier) {
      return null;
    }

    const res = await engineIntentClassifier.run({
      args: [query],
      options,
    });
    // Return the first label from the result
    return res[0].label;
  }

  /**
   * Finds named entities in the query using the NER model.
   * (This has been made public to enable testing)
   *
   * @param {string} query
   *   The user's input query.
   * @param {object} options
   *   The options for the engine pipeline
   * @returns {object[] | null}
   *   The NER results or null if the model is not initialized.
   */
  async _findNER(query, options = {}) {
    const engineNER = this.#modelEngines[this.#getmodelEnginesKey(NER_OPTIONS)];
    return engineNER?.run({ args: [query], options });
  }

  /**
   * Combines location tokens detected by NER into separate city and state
   * components. This method processes city, state, and combined city-state
   * entities, returning an object with `city` and `state` fields.
   *
   * Handles the following entity types:
   * - B-CITY, I-CITY: Identifies city tokens.
   * - B-STATE, I-STATE: Identifies state tokens.
   * - B-CITYSTATE, I-CITYSTATE: Identifies tokens that represent a combined
   *   city and state.
   *
   * @param {object[]} nerResult
   *   The NER results containing tokens and their corresponding entity labels.
   * @param {number} nerThreshold
   *   The confidence threshold for including entities. Tokens with a confidence
   *   score below this threshold will be ignored.
   * @returns {object}
   *   An object with `city` and `state` fields:
   *   - {string|null} city: The detected city, or `null` if no city is found.
   *   - {string|null} state: The detected state, or `null` if no state is found.
   */
  async #combineLocations(nerResult, nerThreshold) {
    let cityResult = [];
    let stateResult = [];
    let cityStateResult = [];

    for (let i = 0; i < nerResult.length; i++) {
      const res = nerResult[i];

      // Handle B-CITY, I-CITY
      if (
        (res.entity === "B-CITY" || res.entity === "I-CITY") &&
        res.score > nerThreshold
      ) {
        if (res.word.startsWith("##") && cityResult.length) {
          cityResult[cityResult.length - 1] += res.word.slice(2);
        } else {
          cityResult.push(res.word);
        }
      }
      // Handle B-STATE, I-STATE
      else if (
        (res.entity === "B-STATE" || res.entity === "I-STATE") &&
        res.score > nerThreshold
      ) {
        if (res.word.startsWith("##") && stateResult.length) {
          stateResult[stateResult.length - 1] += res.word.slice(2);
        } else {
          stateResult.push(res.word);
        }
      }
      // Handle B-CITYSTATE, I-CITYSTATE
      else if (
        (res.entity === "B-CITYSTATE" || res.entity === "I-CITYSTATE") &&
        res.score > nerThreshold
      ) {
        if (res.word.startsWith("##") && cityStateResult.length) {
          cityStateResult[cityStateResult.length - 1] += res.word.slice(2);
        } else {
          cityStateResult.push(res.word);
        }
      }
    }

    // Handle city_state as combined and split into city and state
    if (cityStateResult.length) {
      let cityStateSplit = cityStateResult.join(" ").split(",");
      return {
        city: cityStateSplit[0]?.trim() || null,
        state: cityStateSplit[1]?.trim() || null,
      };
    }

    // Return city and state as separate components if detected
    return {
      city: cityResult.join(" ").trim() || null,
      state: stateResult.join(" ").trim() || null,
    };
  }

  #findSubjectFromQuery(query, location) {
    // If location is null or no city/state, return the entire query
    if (!location || (!location.city && !location.state)) {
      return query;
    }
    // Remove the city and state from the query
    let subjectWithoutLocation = query;
    if (location.city) {
      subjectWithoutLocation = subjectWithoutLocation
        .replace(location.city, "")
        .trim();
    }
    if (location.state) {
      subjectWithoutLocation = subjectWithoutLocation
        .replace(location.state, "")
        .trim();
    }
    // Remove leftover commas, trailing whitespace, and unnecessary punctuation
    subjectWithoutLocation = subjectWithoutLocation
      .replaceAll(",", "")
      .replace(/\s+/g, " ")
      .trim();

    return this.#cleanSubject(subjectWithoutLocation);
  }

  #cleanSubject(subject) {
    let end = PREPOSITIONS.find(
      p => subject === p || subject.endsWith(" " + p)
    );
    if (end) {
      subject = subject.substring(0, subject.length - end.length).trimEnd();
    }
    return subject;
  }

  #sumObjectsByKey(...objs) {
    return objs.reduce((a, b) => {
      for (let k in b) {
        if (b.hasOwnProperty(k)) a[k] = (a[k] || 0) + b[k];
      }
      return a;
    }, {});
  }
}

// Export the singleton instance
export var MLSuggest = new _MLSuggest();
