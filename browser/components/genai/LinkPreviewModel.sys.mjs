/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

// On average, each token represents about 4 characters. A factor of 3.5 is used
// instead of 4 to account for edge cases.
const CHARACTERS_PER_TOKEN = 3.5;
// On average, one token corresponds to approximately 4 characters, meaning 0.25
// times the character count would suffice under normal conditions. To ensure
// robustness and handle edge cases, we use a more conservative factor of 0.69.
const CONTEXT_SIZE_MULTIPLIER = 0.69;
const DEFAULT_INPUT_SENTENCES = 6;
const DEFAULT_OUTPUT_SENTENCES = 3;
const MIN_SENTENCE_LENGTH = 14;
const MIN_WORD_COUNT = 5;

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  createEngine: "chrome://global/content/ml/EngineProcess.sys.mjs",
});
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "config",
  "browser.ml.linkPreview.config",
  "{}"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "inputSentences",
  "browser.ml.linkPreview.inputSentences",
  DEFAULT_INPUT_SENTENCES
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "outputSentences",
  "browser.ml.linkPreview.outputSentences",
  DEFAULT_OUTPUT_SENTENCES
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "prompt",
  "browser.ml.linkPreview.prompt",
  "You're an AI assistant for text re-writing. Rephrase concisely using your own words while preserving its core meaning."
);

export const LinkPreviewModel = {
  /**
   * Extracts sentences from a given text.
   *
   * @param {string} text text to process
   * @returns {Array<string>} sentences
   */
  getSentences(text) {
    const abbreviations = [
      "Mr.",
      "Mrs.",
      "Ms.",
      "Dr.",
      "Prof.",
      "Inc.",
      "Ltd.",
      "Jr.",
      "Sr.",
      "St.",
      "e.g.",
      "i.e.",
      "U.S.A",
      "D.C.",
      "U.K.",
      "etc.",
      "a.m.",
      "p.m.",
      "D.",
      "Mass.",
      "Sen.",
      "Rep.",
      "No.",
      "Fig.",
      "vs.",
      "Mx.",
      "Ph.D.",
      "M.D.",
      "D.D.S.",
      "B.A.",
      "M.A.",
      "LL.B.",
      "LL.M.",
      "J.D.",
      "D.O.",
      "D.V.M.",
      "Psy.D.",
      "Ed.D.",
      "Eng.",
      "Co.",
      "Corp.",
      "Mt.",
      "Ft.",
      "U.S.",
      "U.S.A.",
      "E.U.",
      "et al.",
      "Nos.",
      "pp.",
      "Vol.",
      "Rev.",
      "Gen.",
      "Lt.",
      "Col.",
      "Maj.",
      "Capt.",
      "Sgt.",
      "Cpl.",
      "Pvt.",
      "Adm.",
      "Cmdr.",
      "Ave.",
      "Blvd.",
      "Rd.",
      "Ln.",
      "Jan.",
      "Feb.",
      "Mar.",
      "Apr.",
      "May.",
      "Jun.",
      "Jul.",
      "Aug.",
      "Sep.",
      "Sept.",
      "Oct.",
      "Nov.",
      "Dec.",
      "Mon.",
      "Tue.",
      "Tues.",
      "Wed.",
      "Thu.",
      "Thur.",
      "Thurs.",
      "Fri.",
      "Sat.",
      "Sun.",
      "Dept.",
      "Univ.",
      "Est.",
      "Calif.",
      "Fla.",
      "N.Y.",
      "Conn.",
      "Va.",
      "Ill.",
      "Assoc.",
      "Bros.",
      "Dist.",
      "Msgr.",
      "S.P.",
      "P.S.",
      "U.S.S.R.",
      "Mlle.",
      "Mme.",
      "Hon.",
      "Messrs.",
      "Mmes.",
      "v.",
      "vs.",
    ];

    // Replace periods in abbreviations with a placeholder.
    let modifiedText = text;
    const placeholder = "∯";

    abbreviations.forEach(abbrev => {
      const escapedAbbrev = abbrev
        .replace(/[.*+?^${}()|[\]\\]/g, "\\$&")
        .replace(/\\\./g, "\\.");
      const regex = new RegExp(escapedAbbrev, "g");
      const abbrevWithPlaceholder = abbrev.replace(/\./g, placeholder);
      modifiedText = modifiedText.replace(regex, abbrevWithPlaceholder);
    });

    const segmenter = new Intl.Segmenter("en", {
      granularity: "sentence",
    });
    const segments = segmenter.segment(modifiedText);
    let sentences = Array.from(segments, segment => segment.segment);

    // Restore the periods in abbreviations.
    return sentences.map(sentence =>
      sentence.replace(new RegExp(placeholder, "g"), ".")
    );
  },

  /**
   * Clean up text for text generation AI.
   *
   * @param {string} text to process
   * @returns {string} cleaned up text
   */
  preprocessText(text) {
    return (
      this.getSentences(text)
        .map(s => s.trim())
        // Remove sentences that are too short without punctuation.
        .filter(
          s =>
            s.length >= MIN_SENTENCE_LENGTH &&
            s.split(" ").length >= MIN_WORD_COUNT &&
            /\p{P}$/u.test(s)
        )
        .slice(0, lazy.inputSentences)
        .join(" ")
    );
  },

  /**
   * Generate summary text using AI.
   *
   * @param {string} inputText
   * @param {object} callbacks for progress and error
   * @param {Function} callbacks.onText optional for text chunks
   * @param {Function} callbacks.onError optional for error
   */
  async generateTextAI(inputText, { onText, onError } = {}) {
    const processedInput = this.preprocessText(inputText);
    // Asssume generated text is approximately the same length as the input.
    const nPredict = Math.ceil(processedInput.length / CHARACTERS_PER_TOKEN);
    const systemPrompt = lazy.prompt;
    // Estimate an upper bound for the required number of tokens. This estimate
    // must be large enough to include prompt tokens, input tokens, and
    // generated tokens.
    const numContext =
      Math.ceil(
        (processedInput.length + systemPrompt.length) * CONTEXT_SIZE_MULTIPLIER
      ) + nPredict;

    let engine;
    try {
      engine = await lazy.createEngine({
        backend: "wllama",
        engineId: "wllamapreview",
        kvCacheDtype: "q8_0",
        modelFile: "smollm2-360m-instruct-q8_0.gguf",
        modelHubRootUrl: "https://model-hub.mozilla.org",
        modelHubUrlTemplate: "{model}/{revision}",
        modelId: "HuggingFaceTB/SmolLM2-360M-Instruct-GGUF",
        modelRevision: "main",
        numBatch: numContext,
        numContext,
        numUbatch: numContext,
        runtimeFilename: "wllama.wasm",
        taskName: "wllama-text-generation",
        timeoutMS: -1,
        useMlock: false,
        useMmap: true,
        ...JSON.parse(lazy.config),
      });

      const postProcessor = new SentencePostProcessor();
      for await (const val of engine.runWithGenerator({
        nPredict,
        prompt: [
          { role: "system", content: systemPrompt },
          { role: "user", content: processedInput },
        ],
      })) {
        const sentence = postProcessor.put(val.text);
        if (sentence) {
          onText?.(sentence);
        } else if (!val.text) {
          const remaining = postProcessor.flush();
          if (remaining) {
            onText?.(remaining);
          }
        }
      }
    } catch (error) {
      onError?.(error);
    } finally {
      await engine?.terminate();
    }
  },
};

/**
 * A class for processing streaming text to detect and extract complete
 * sentences. It buffers incoming text and periodically checks for new sentences
 * based on punctuation and character count limits.
 *
 * This class is useful for incremental sentence processing in NLP tasks.
 */
export class SentencePostProcessor {
  /**
   * The maximum number of sentences to output before truncating the buffer.
   * Use -1 for unlimited.
   *
   * @type {number}
   */
  maxNumOutputSentences = -1;

  /**
   * Stores the current text being processed.
   *
   * @type {string}
   */
  currentText = "";

  /**
   * Tracks the number of sentences processed so far.
   *
   * @type {number}
   */
  currentNumSentences = 0;

  /**
   * @param {number} maxNumOutputSentences - The maximum number of sentences to
   * output before truncating the buffer.
   */
  constructor(maxNumOutputSentences = lazy.outputSentences) {
    this.maxNumOutputSentences = maxNumOutputSentences;
  }

  /**
   * Processes incoming text, checking if a full sentence has been completed. If
   * a full sentence is detected, it returns the first complete sentence.
   * Otherwise, it returns an empty string.
   *
   * @param {string} text to process
   * @returns {string} first complete sentence if available, otherwise ""
   */
  put(text) {
    if (this.currentNumSentences == this.maxNumOutputSentences) {
      return "";
    }
    this.currentText += text;

    // We need to ensure that the current sentence is complete and the next
    // has started before reporting that a sentence is ready.
    const sentences = LinkPreviewModel.getSentences(this.currentText);
    if (sentences.length >= 2) {
      this.currentText = sentences.slice(1).join("");
      this.currentNumSentences += 1;

      if (this.currentNumSentences == this.maxNumOutputSentences) {
        this.currentText = "";
      }
      return sentences[0];
    }

    return "";
  }

  /**
   * Flushes the remaining text buffer. This ensures that any last remaining
   * sentence is returned.
   *
   * @returns {string} remaining text that hasn't been processed yet
   */
  flush() {
    return this.currentText;
  }
}
