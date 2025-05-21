/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * @typedef {object} Lazy
 * @property {typeof setTimeout} setTimeout
 * @property {typeof clearTimeout} clearTimeout
 * @property {typeof console} console
 * @property {typeof import("chrome://global/content/translations/TranslationsUtils.mjs").TranslationsUtils} TranslationsUtils
 */

/** @type {Lazy} */
const lazy = /** @type {any} */ ({});

ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  TranslationsUtils:
    "chrome://global/content/translations/TranslationsUtils.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

/**
 * Map the NodeFilter enums that are used by the TreeWalker into enums that make
 * sense for determining the status of the nodes for the TranslationsDocument process.
 * This aligns the meanings of the filtering for the translations process.
 */
const NodeStatus = {
  // This node is ready to translate as is.
  READY_TO_TRANSLATE: NodeFilter.FILTER_ACCEPT,

  // This node is a shadow host and needs to be subdivided further.
  SHADOW_HOST: NodeFilter.FILTER_ACCEPT,

  // This node contains too many block elements and needs to be subdivided further.
  SUBDIVIDE_FURTHER: NodeFilter.FILTER_SKIP,

  // This node should not be considered for translation.
  NOT_TRANSLATABLE: NodeFilter.FILTER_REJECT,
};

/**
 * @typedef {import("../translations").NodeVisibility} NodeVisibility
 * @typedef {import("../translations").LanguagePair} LanguagePair
 * @typedef {import("../translations").PortToPage} PortToPage
 * @typedef {import("../translations").EngineStatus} EngineStatus
 * @typedef {(message: string) => Promise<string>} TranslationFunction
 */

/**
 * This contains all of the information needed to perform a translation request.
 *
 * @typedef {object} TranslationRequest
 * @property {Node} node
 * @property {string} sourceText
 * @property {number} translationId
 * @property {boolean} isHTML
 * @property {(translation: Promise<string> | string | null) => unknown} resolve
 * @property {(reason: any) => unknown} reject
 */

/**
 * Create a translation cache with a limit. It implements a "least recently used" strategy
 * to remove old translations. After `#cacheExpirationMS` the cache will be emptied.
 * This cache is owned statically by the TranslationsChild. This means that it will be
 * re-used on page reloads if the origin of the site does not change.
 */
export class LRUCache {
  /**
   * A Map from input HTML strings to their translated HTML strings.
   *
   * This cache is used to check if we already have a translated response for the given
   * input HTML, to help avoid spending CPU cycles translating HTML for which we already
   * know the translated output.
   *
   * @type {Map<string, string>}
   */
  #htmlCacheMap = new Map();

  /**
   * A Map from input text strings to their translated text strings.
   *
   * This cache is used to check if we already have a translated response for the given
   * input text, to help avoid spending CPU cycles translating text for which we already
   * know the translated output.
   *
   * @type {Map<string, string>}
   */
  #textCacheMap = new Map();

  /**
   * A Set containing strings of translated plain text output.
   *
   * This cache is used to check if the text has already been translated,
   * to help avoid sending already-translated text to be translated a second time.
   *
   * Ideally, a translation model that receives source text that is already in the
   * target translation language should just pass it through, but this is not always
   * the case in practice. Depending on the model, sending already-translated text to
   * be translated again may change the translation or even produce garbage as a response.
   *
   * Best to avoid this situation altogether if we can.
   *
   * @type {Set<string>}
   */
  #textCacheSet = new Set();

  /**
   * The language pair for this cache. All cached translations will be for the given pair.
   *
   * @type {LanguagePair}
   */
  #languagePair;

  /**
   * The limit of entries that can be held in each underlying cache before old entries
   * will start being replaced by new entries.
   *
   * @type {number}
   */
  #cacheLimit = 5_000;

  /**
   * This cache will self-destruct after 10 minutes.
   *
   * @type {number}
   */
  #cacheExpirationMS = 10 * 60_000;

  /**
   * The source and target langue pair for the content in this cache.
   *
   * @param {LanguagePair} languagePair
   */
  constructor(languagePair) {
    this.#languagePair = languagePair;
  }

  /**
   * Retrieves the corresponding Map from source text to translated text.
   *
   * This is used to determine if a cached translation already exists for
   * the given source text, preventing us from having to spend CPU time by
   * recomputing the translation.
   *
   * @param {boolean} isHTML
   *
   * @returns {Map<string, string>}
   */
  #getCacheMap(isHTML) {
    return isHTML ? this.#htmlCacheMap : this.#textCacheMap;
  }

  /**
   * Get a translation if it exists from the cache, and move it to the end of the cache
   * to keep it alive longer.
   *
   * @param {string} sourceString
   * @param {boolean} isHTML
   *
   * @returns {string | undefined}
   */
  get(sourceString, isHTML) {
    const cacheMap = this.#getCacheMap(isHTML);
    const targetString = cacheMap.get(sourceString);

    if (targetString === undefined) {
      return undefined;
    }

    // Maps are ordered, move this item to the end of the list so it will stay
    // alive longer.
    cacheMap.delete(sourceString);
    cacheMap.set(sourceString, targetString);

    this.keepAlive();

    return targetString;
  }

  /**
   * Adds a new translation to the cache, a mapping from the source text to the target text.
   *
   * @param {string} sourceString
   * @param {string} targetString
   * @param {boolean} isHTML
   */
  set(sourceString, targetString, isHTML) {
    const cacheMap = this.#getCacheMap(isHTML);
    if (cacheMap.has(sourceString)) {
      // The Map already has this value, so we must delete it to
      // re-insert it at the most-recently-used position of the Map.
      cacheMap.delete(sourceString);
    } else if (cacheMap.size === this.#cacheLimit) {
      // The Map is at capacity, so we must evict the least-recently-used value.
      const oldestKey = cacheMap.keys().next().value;
      // @ts-ignore: We can ensure that oldestKey is not undefined.
      cacheMap.delete(oldestKey);
    }
    cacheMap.set(sourceString, targetString);

    if (!isHTML) {
      if (this.#textCacheSet.has(targetString)) {
        // The Set already has this value, so we must delete it to
        // re-insert it at the most-recently-used position of the Set.
        this.#textCacheSet.delete(targetString);
      } else if (this.#textCacheSet.size === this.#cacheLimit) {
        // The Set is at capacity, so we must evict the least-recently-used value.
        const oldestKey = this.#textCacheSet.keys().next().value;
        // @ts-ignore: We can ensure that oldestKey is not undefined.
        this.#textCacheSet.delete(oldestKey);
      }
      this.#textCacheSet.add(targetString);
    }

    this.keepAlive();
  }

  /**
   * Returns true if the source text is text that has already been translated
   * into the target language, otherwise false. If so, we want to avoid sending
   * this text to be translated a second time. Depending on the model, retranslating
   * text that is already in the target language may produce garbage output.
   *
   * @param {string} sourceText
   * @returns {boolean}
   */
  isAlreadyTranslated(sourceText) {
    return this.#textCacheSet.has(sourceText);
  }

  /**
   * Returns true if the given pair matches the language pair for this cache, otherwise false.
   *
   * @param {LanguagePair} languagePair
   *
   * @returns {boolean}
   */
  matches(languagePair) {
    return (
      lazy.TranslationsUtils.langTagsMatch(
        this.#languagePair.sourceLanguage,
        languagePair.sourceLanguage
      ) &&
      lazy.TranslationsUtils.langTagsMatch(
        this.#languagePair.targetLanguage,
        languagePair.targetLanguage
      )
    );
  }

  /**
   * The id for the cache's keep-alive timeout, at which point it will destroy itself.
   *
   * @type {number}
   */
  #keepAliveTimeoutId = 0;

  /**
   * Used to ensure that only one callback is added to the event loop to set keep-alive timeout.
   *
   * @type {boolean}
   */
  #hasPendingKeepAliveCallback = false;

  /**
   * Resets the timer for the cache's keep-alive timeout, extending the time the cache will live.
   */
  keepAlive() {
    if (this.#hasPendingKeepAliveCallback) {
      // There is already a pending callback to extend the timeout.
      return;
    }

    if (this.#keepAliveTimeoutId) {
      lazy.clearTimeout(this.#keepAliveTimeoutId);
      this.#keepAliveTimeoutId = 0;
    }

    this.#hasPendingKeepAliveCallback = true;
    lazy.setTimeout(() => {
      this.#hasPendingKeepAliveCallback = false;
      this.#keepAliveTimeoutId = lazy.setTimeout(() => {
        this.#htmlCacheMap = new Map();
        this.#textCacheMap = new Map();
        this.#textCacheSet = new Set();
      }, this.#cacheExpirationMS);
    }, 0);
  }
}

/**
 * How often the DOM is updated with translations, in milliseconds.
 *
 * Each time the DOM is updated, we must pause the mutation observer.
 *
 *  - Stopping the observer takes about 5 micro seconds based on profiling.
 *
 *  - Starting the observer takes about 30 micro seconds based on profiling.
 *
 * We want to choose a DOM update interval that is fast enough to feel instantaneously
 * reactive when completed translation requests come in, while also allowing multiple
 * nodes to be updated within a single pause of the observer.
 *
 * @type {number}
 */
const DOM_UPDATE_INTERVAL_MS = 50;

/**
 * Tags excluded from content translation.
 */
const CONTENT_EXCLUDED_TAGS = new Set([
  // The following are elements that semantically should not be translated.
  "CODE",
  "KBD",
  "SAMP",
  "VAR",
  "ACRONYM",

  // The following are deprecated tags.
  "DIR",
  "APPLET",

  // The following are embedded elements, and are not supported (yet).
  "MATH",
  "EMBED",
  "OBJECT",
  "IFRAME",

  // This is an SVG tag that can contain arbitrary XML, ignore it.
  "METADATA",

  // These are elements that are treated as opaque by Firefox which causes their
  // innerHTML property to be just the raw text node behind it. Any text that is sent as
  // HTML must be valid, and there is no guarantee that the innerHTML is valid.
  "NOSCRIPT",
  "NOEMBED",
  "NOFRAMES",

  // The title is handled separately, and a HEAD tag should not be considered.
  "HEAD",

  // These are not user-visible tags.
  "STYLE",
  "SCRIPT",
  "TEMPLATE",

  // Textarea elements contain user content, which should not be translated.
  "TEXTAREA",
]);

/**
 * Tags excluded from attribute translation.
 */
const ATTRIBUTE_EXCLUDED_TAGS = (() => {
  const attributeTags = new Set(CONTENT_EXCLUDED_TAGS);

  // The <head> element may contain <meta> elements that may have translatable attributes.
  // So we will allow <head> for attribute translations, but not for content translations.
  attributeTags.delete("HEAD");

  // <textarea> elements are excluded from content translation, because we do not want to
  // translate text that the user types, but the "placeholder"attribute should be translated.
  attributeTags.delete("TEXTAREA");

  return attributeTags;
})();

/**
 * A map of criteria to determine if an attribute is translatable for a given element.
 * Each key in the map represents an attribute name, while the value can be either `null` or an array of further criteria.
 *
 * - If the criteria value is `null`, the attribute is considered translatable for any element.
 *
 * - If the criteria array is specified, then at least one criterion must match a given element in order for the attribute to be translatable.
 *   Each object in the array defines a tagName and optional conditions to match against an element in question.
 *
 *   - If none of the tagNames match the element, then the attribute is not translatable for that element.
 *
 *   - If a tagName matches and no further conditions are specified, then the attribute is always translatable for elements of that type.
 *
 *   - If a tagName matches and further conditions are specified, then at least one of the conditions must match for the attribute to be translatable for that element.
 *
 * Example:
 *
 * - "title" is translatable for all elements.
 *
 * - "label" is translatable only for "TRACK" elements.
 *
 * - "value" is translatable only for "INPUT" elements whose "type" attribute is "button", "reset".
 *
 * @type {Map<string, Array<{ tagName: string, conditions?: Record<string, Array<string>> }> | null>}
 */
const TRANSLATABLE_ATTRIBUTES = new Map([
  ["abbr", [{ tagName: "TH" }]],
  [
    "alt",
    [
      { tagName: "AREA" },
      { tagName: "IMAGE" },
      { tagName: "IMG" },
      { tagName: "INPUT" },
    ],
  ],
  ["aria-braillelabel", null],
  ["aria-brailleroledescription", null],
  ["aria-colindextext", null],
  ["aria-description", null],
  ["aria-label", null],
  ["aria-placeholder", null],
  ["aria-roledescription", null],
  ["aria-rowindextext", null],
  ["aria-valuetext", null],
  [
    "content",
    [{ tagName: "META", conditions: { name: ["description", "keywords"] } }],
  ],
  ["download", [{ tagName: "A" }, { tagName: "AREA" }]],
  [
    "label",
    [{ tagName: "TRACK" }, { tagName: "OPTGROUP" }, { tagName: "OPTION" }],
  ],
  ["placeholder", [{ tagName: "INPUT" }, { tagName: "TEXTAREA" }]],
  ["title", null],
  [
    // We only want to translate value attributes for button-like <input> elements.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1919230#c10
    // type: submit is not translated because it may affect form submission, depending on how the server is configured.
    // See https://github.com/whatwg/html/issues/3396#issue-291182587
    "value",
    [{ tagName: "INPUT", conditions: { type: ["button", "reset"] } }],
  ],
]);

/**
 * A single CSS selector string that matches elements with the criteria defined in TRANSLATABLE_ATTRIBUTES.
 *
 * @see TRANSLATABLE_ATTRIBUTES
 *
 * @type {string}
 */
const TRANSLATABLE_ATTRIBUTES_SELECTOR = (() => {
  const selectors = [];

  for (const [attribute, criteria] of TRANSLATABLE_ATTRIBUTES) {
    if (!criteria) {
      // There are no further criteria: we translate this attribute for all elements.
      // Example: [title]
      selectors.push(`[${attribute}]`);
      continue;
    }

    for (const { tagName, conditions } of criteria) {
      if (!conditions) {
        // There are no further conditions: we translate this attribute for all elements with this tagName.
        // Example: TRACK[label]
        selectors.push(`${tagName}[${attribute}]`);
        continue;
      }

      // Further conditions are specified, so we must add a selector for each condition.
      for (const [key, values] of Object.entries(conditions)) {
        for (const value of values) {
          // Example: INPUT[value][type="button"]
          selectors.push(`${tagName}[${attribute}][${key}="${value}"]`);
        }
      }
    }
  }

  return selectors.join(",");
})();

/**
 * Options used by the mutation observer
 */
const MUTATION_OBSERVER_OPTIONS = {
  characterData: true,
  childList: true,
  subtree: true,
  attributes: true,
  attributeOldValue: true,
  attributeFilter: [...TRANSLATABLE_ATTRIBUTES.keys()],
};

/**
 * This class manages the process of translating the DOM from one language to another.
 * A translateHTML and a translateText function are injected into the constructor. This
 * class is responsible for subdividing a Node into small enough pieces to where it
 * contains a reasonable amount of text and inline elements for the translations engine
 * to translate. Once a node has been identified as a small enough chunk, its innerHTML
 * is read, and sent for translation. The async translation result comes back as an HTML
 * string. The DOM node is updated with the new text and potentially changed DOM ordering.
 *
 * This class also handles mutations of the DOM and will translate nodes as they are added
 * to the page, or the when the node's text is changed by content scripts.
 *
 * Flow for discarding translations due to mutations:
 * ==================================================
 *
 * This diagram shows the flow of translations and how to discard translations when there
 * are mutations, which can happen at any point after a translation is requested, up to
 * the point a node is updated.
 *                                                        [discard]        [discard]
 *                                                            ^                ^
 *                                                            │ mutated?       │ mutated?
 *  Document: ┌─────────────┐                             ┌────────┐       ┌────────┐
 *            │   request   │      wait for response      │ queue  │       │ update │
 *            │ translation │ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ > │ update │ ────> │  node  │
 *            └─────────────┘                             └────────┘       └────────┘
 *                 │                                          ^
 *                 v                                          │
 *   Worker:  ┌─────────────┐       ┌───────────┐        ┌──────────┐
 *            │   add to    │       │  runTask  │        │   post   │
 *            │  WorkQueue  │ ────> │ translate │  ────> │ response │
 *            └─────────────┘       └───────────┘        └──────────┘
 *            ^                     ^
 *            └─────────────────────┘
 *            Handle discard requests
 */
export class TranslationsDocument {
  /**
   * The BCP 47 language tag that matches the page's source language.
   *
   * If elements are found that do not match this language, then they are skipped,
   * because our translation models only operate between the exact language pair.
   *
   * @type {string}
   */
  #documentLanguage;

  /**
   * The timeout between the first translation received and the call to update the DOM
   * with translations.
   *
   * @type {null | number}
   */
  #hasPendingUpdateContentCallback = null;

  /**
   * @type {null | number}
   */
  #hasPendingUpdateAttributesCallback = null;

  /**
   * The nodes that need translations. They are queued when the document tree is walked,
   * and then they are dispatched for translation based on their visibility. The viewport
   * nodes are given the highest priority.
   *
   * @type {Map<Node, NodeVisibility>}
   */
  #queuedContentNodes = new Map();

  /**
   * The nodes that need Attribute translations. They are queued when the document tree is walked,
   * and then they are dispatched for translation based on their visibility. The viewport
   * nodes are given the highest priority.
   *
   * @type  {Map<Element, { attributeSet: Set<string>, visibility: NodeVisibility }>}
   */
  #queuedAttributeElements = new Map();

  /**
   * The list of nodes that need updating with the translated HTML. These are batched
   * into an update. The translationId is a monotonically increasing number that
   * represents a unique id for a translation. It guards against races where a node is
   * mutated before the translation is returned. The translation is asynchronously
   * cancelled during a mutation, but it can still return a translation before it is
   * cancelled.
   *
   * @type {Set<{ node: Node, translatedContent: string, translationId: number }>}
   */
  #nodesWithTranslatedContent = new Set();

  /**
   * The list of nodes that need updating with the translated Attribute HTML. These are batched
   * into an update.
   *
   * @type {Set<{ element: Element, translation: string, attribute: string, translationId: number }>}
   */
  #elementsWithTranslatedAttributes = new Set();

  /**
   * The set of nodes that have been subdivided and processed for translation. They
   * should not be submitted again unless their contents have been changed.
   *
   * @type {WeakSet<Node>}
   */
  #processedContentNodes = new WeakSet();

  /**
   * All root elements we're trying to translate. This should be the `document.body`
   * the <head> (for attributes only), and the <title> element.
   *
   * @type {Set<Node>}
   */
  #rootNodes = new Set();

  /**
   * A collection of nodes whose text content has mutated, which will be batched
   * together and sent to be re-translated once every requestAnimationFrame.
   *
   * @type {Set<Node>}
   */
  #nodesWithMutatedContent = new Set();

  /**
   * A collection of elements whose attributes have mutated, which will be batched
   * together and sent to be re-translated once every requestAnimationFrame.
   *
   * @type {Map<Element, Set<string>>}
   */
  #elementsWithMutatedAttributes = new Map();

  /**
   * Marks when we have a pending callback for updating the mutated nodes.
   * This ensures that we won't redundantly request for nodes to be updated.
   *
   * @type {boolean}
   */
  #hasPendingMutatedNodesCallback = false;

  /**
   * This boolean indicates whether the first visible DOM translation change is about to occur.
   *
   * @type {boolean}
   */
  #hasFirstVisibleChange = false;

  /**
   * A unique ID that guards against races between translations and mutations.
   *
   * @type {Map<Node, number>}
   */
  #pendingContentTranslations = new Map();

  /**
   * A unique ID that guards against races between translations and mutations. The
   * Map<string, number> is a mapping of the node's attribute to the translation id.
   *
   * @type {Map<Element, Map<string, number>>}
   */
  #pendingAttributeTranslations = new Map();

  /**
   * Cache a map of all child nodes to their pending parents. This lookup was slow
   * from profiling sites like YouTube with lots of mutations. Caching the relationship
   * speeds it up.
   *
   * @type {WeakMap<Node, Node>}
   */
  #nodeToPendingParent = new WeakMap();

  /**
   * Start with 1 so that it will never be falsey.
   *
   * @type {number}
   */
  #lastTranslationId = 1;

  /**
   * A cache of recent translations, used to avoid wasting CPU time translating text
   * for which we already have a translated response.
   *
   * @type {LRUCache}
   */
  #translationsCache;

  /**
   * The DOMParser is used when updating elements with translated text.
   *
   * @type {DOMParser}
   */
  #domParser;

  /**
   * The mutation observer that watches for both new and mutated nodes.
   *
   * @type {MutationObserver}
   */
  #mutationObserver;

  /**
   * The inner-window ID is used for better profiler marker reporting.
   *
   * @type {number}
   */
  #innerWindowId;

  /**
   * The original document of the page that we will be updating with translated text.
   *
   * @type {Document}
   */
  #sourceDocument;

  /**
   * A callback that will report that the first visible change has been made to the page.
   * This is a key performance metric when considering the time to initialize translations.
   *
   * @type {() => void}
   */
  #actorReportFirstVisibleChange;

  /**
   * Construct a new TranslationsDocument. It is tied to a specific Document and cannot
   * be re-used. The translation functions are injected since this class shouldn't
   * manage the life cycle of the translations engines.
   *
   * @param {Document} document
   * @param {string} documentLanguage - The BCP 47 tag of the source language.
   * @param {string} targetLanguage - The BCP 47 tag of the destination language.
   * @param {number} innerWindowId - This is used for better profiler marker reporting.
   * @param {MessagePort} port - The port to the translations engine.
   * @param {() => void} requestNewPort - Used when an engine times out and a new
   *                                      translation request comes in.
   * @param {() => void} reportVisibleChange - Used to report to the actor that the first visible change
   *                                           for a translation is about to occur.
   * @param {LRUCache} translationsCache - A cache in which to store translated text.
   */
  constructor(
    document,
    documentLanguage,
    targetLanguage,
    innerWindowId,
    port,
    requestNewPort,
    reportVisibleChange,
    translationsCache
  ) {
    /** @type {WindowProxy} */
    const ownerGlobal = ensureExists(document.ownerGlobal);

    this.#domParser = new ownerGlobal.DOMParser();
    this.#innerWindowId = innerWindowId;
    this.#sourceDocument = document;
    this.#documentLanguage = documentLanguage;
    this.#translationsCache = translationsCache;
    this.#actorReportFirstVisibleChange = reportVisibleChange;

    /**
     * This selector runs to find child nodes that should be excluded. It should be
     * basically the same implementation of `isExcludedNode`, but as a selector.
     *
     * @type {string}
     */
    this.contentExcludedNodeSelector = [
      // Use: [lang|=value] to match language codes.
      //
      // Per: https://developer.mozilla.org/en-US/docs/Web/CSS/Attribute_selectors
      //
      // The elements with an attribute name of attr whose value can be exactly
      // value or can begin with value immediately followed by a hyphen, - (U+002D).
      // It is often used for language subcode matches.
      `[lang]:not([lang|="${this.#documentLanguage}"])`,
      `[translate=no]`,
      `.notranslate`,
      `[contenteditable="true"]`,
      `[contenteditable=""]`,
      [...CONTENT_EXCLUDED_TAGS].join(","),
    ].join(",");

    /**
     * This selector runs to find elements that should be excluded from attribute translation.
     *
     * @type {string}
     */
    this.attributeExcludedNodeSelector = [
      // Exclude any element with translate="no", as it explicitly opts out of translation.
      `[translate="no"]`,

      // Exclude any element that is a descendant of a container marked with "notranslate" class.
      `.notranslate`,
      [...ATTRIBUTE_EXCLUDED_TAGS].join(","),
    ].join(",");

    /**
     * Define the type of the MutationObserver for editor type hinting.
     *
     * @type {typeof MutationObserver}
     */
    const DocumentMutationObserver = ownerGlobal.MutationObserver;

    this.#mutationObserver = new DocumentMutationObserver(mutationsList => {
      for (const mutation of mutationsList) {
        if (!mutation.target) {
          continue;
        }

        const pendingNode = this.#getPendingNodeFromTarget(mutation.target);
        if (pendingNode) {
          if (this.#preventContentTranslation(pendingNode)) {
            // The node was still pending to be translated, cancel it and re-submit.
            this.#markNodeContentMutated(pendingNode);
            if (mutation.type === "childList") {
              // New nodes could have been added, make sure we can follow their shadow roots.
              ensureExists(
                this.#sourceDocument.ownerGlobal
              ).requestAnimationFrame(() => {
                this.#addShadowRootsToObserver(pendingNode);
              });
            }
            continue;
          }
        }

        switch (mutation.type) {
          case "childList": {
            for (const addedNode of mutation.addedNodes) {
              if (!addedNode) {
                continue;
              }
              this.#addShadowRootsToObserver(addedNode);
              this.#markNodeContentMutated(addedNode);
            }
            for (const removedNode of mutation.removedNodes) {
              if (!removedNode) {
                continue;
              }
              this.#preventContentTranslation(removedNode);
              this.#preventAttributeTranslations(removedNode);
            }
            break;
          }
          case "characterData": {
            const node = mutation.target;
            if (node) {
              // The mutated node will implement the CharacterData interface. The only
              // node of this type that contains user-visible text is the `Text` node.
              // Ignore others such as the comment node.
              // https://developer.mozilla.org/en-US/docs/Web/API/CharacterData
              if (node.nodeType === Node.TEXT_NODE) {
                this.#markNodeContentMutated(node);
              }
            }
            break;
          }
          case "attributes": {
            const element = asElement(mutation.target);
            if (element && mutation.attributeName) {
              const { oldValue, attributeName } = mutation;
              const newValue = element.getAttribute(attributeName);

              if (
                // The new attribute value must have content to translate.
                newValue?.length &&
                // The new attribute value must not be exactly the same as the old value.
                oldValue !== newValue &&
                // The new attribute value must not be already-translated text.
                !this.#translationsCache.isAlreadyTranslated(newValue)
              ) {
                this.#maybeMarkElementAttributeMutated(element, attributeName);
              }
            }
            break;
          }
          default: {
            break;
          }
        }
      }
    });

    this.#sourceDocument.addEventListener(
      "visibilitychange",
      this.#handleVisibilityChange
    );

    const addRootElements = () => {
      this.#addRootElement(document.querySelector("title"));
      this.#addRootElement(document.head);
      this.#addRootElement(document.body);
    };

    if (document.body) {
      addRootElements();
    } else {
      // The TranslationsDocument was invoked before the DOM was ready, wait for
      // it to be loaded.
      document.addEventListener("DOMContentLoaded", addRootElements);
    }

    /** @type {HTMLElement} */ (document.documentElement).lang = targetLanguage;

    lazy.console.log(
      "Beginning to translate.",
      // The defaultView may not be there on tests.
      document.defaultView?.location.href
    );
  }

  /**
   * Marks that the text content of the given node has mutated, both allowing and
   * ensuring that the node will be rescheduled for translation, even if it had
   * previously been translated.
   *
   * @param {Node} node
   */
  #markNodeContentMutated(node) {
    this.#processedContentNodes.delete(node);
    this.#nodesWithMutatedContent.add(node);
    this.#ensureMutationUpdateCallbackIsRegistered();
  }

  /**
   * Marks that the given element's attribute has been mutated, only if that attribute
   * is translatable for that element, both allowing and ensuring that the attribute will
   * be rescheduled for translation, even if it had previously been translated.
   *
   * @param {Element} element
   * @param {string} attributeName
   */
  #maybeMarkElementAttributeMutated(element, attributeName) {
    if (!isAttributeTranslatable(element, attributeName)) {
      // The given attribute is not translatable for this element.
      return;
    }

    let attributes = this.#elementsWithMutatedAttributes.get(element);
    if (!attributes) {
      attributes = new Set();
      this.#elementsWithMutatedAttributes.set(element, attributes);
    }
    attributes.add(attributeName);
    this.#ensureMutationUpdateCallbackIsRegistered();
  }

  /**
   * Ensures that all nodes that have been picked up by the mutation observer
   * are processed, prioritized and sent to the scheduler to re translated.
   */
  #ensureMutationUpdateCallbackIsRegistered() {
    if (this.#hasPendingMutatedNodesCallback) {
      // A callback has already been registered to update mutated nodes.
      return;
    }

    if (
      this.#nodesWithMutatedContent.size === 0 &&
      this.#elementsWithMutatedAttributes.size === 0
    ) {
      // There are no mutated nodes to update.
      return;
    }

    this.#hasPendingMutatedNodesCallback = true;
    const ownerGlobal = ensureExists(this.#sourceDocument.ownerGlobal);

    // Nodes can be mutated in a tight loop. To guard against the performance of re-translating nodes too frequently,
    // we will batch the processing of mutated nodes into a double requestAnimationFrame.
    ownerGlobal.requestAnimationFrame(() => {
      ownerGlobal.requestAnimationFrame(() => {
        this.#hasPendingMutatedNodesCallback = false;

        // Ensure the nodes are still alive.
        const liveNodes = [];
        for (const node of this.#nodesWithMutatedContent) {
          if (isNodeDetached(node)) {
            this.#nodesWithMutatedContent.delete(node);
          } else {
            liveNodes.push(node);
          }
        }

        // Remove any nodes that are contained in another node.
        for (let i = 0; i < liveNodes.length; i++) {
          const node = liveNodes[i];
          if (!this.#nodesWithMutatedContent.has(node)) {
            continue;
          }
          for (let j = i + 1; j < liveNodes.length; j++) {
            const otherNode = liveNodes[j];

            if (!this.#nodesWithMutatedContent.has(otherNode)) {
              continue;
            }

            if (node.contains(otherNode)) {
              this.#nodesWithMutatedContent.delete(otherNode);
            } else if (otherNode.contains(node)) {
              this.#nodesWithMutatedContent.delete(node);
              break;
            }
          }
        }

        for (const node of this.#nodesWithMutatedContent) {
          this.#addShadowRootsToObserver(node);
          this.#subdivideNodeForContentTranslations(node);
          this.#subdivideNodeForAttributeTranslations(node);
        }
        this.#nodesWithMutatedContent.clear();

        for (const [
          node,
          attributes,
        ] of this.#elementsWithMutatedAttributes.entries()) {
          this.#enqueueElementForAttributeTranslation(node, attributes);
        }
        this.#dispatchQueuedAttributeTranslations();
        this.#elementsWithMutatedAttributes.clear();
      });
    });
  }

  /**
   * If a pending node contains or is the target node, return that pending node.
   *
   * @param {Node} target
   *
   * @returns {Node | undefined}
   */
  #getPendingNodeFromTarget(target) {
    return this.#nodeToPendingParent.get(target);
  }

  /**
   * Attempts to cancel a translation for the given node, even if the relevant
   * translation request has already been sent to the TranslationsEngine.
   *
   * This function is primarily used by the mutation observer, when we are certain
   * that content has changed, and the previous translation is no longer valid.
   *
   * @param {Node} node
   * @returns {boolean}
   */
  #preventContentTranslation(node) {
    const translationId = this.#pendingContentTranslations.get(node);

    if (!translationId) {
      // No pending content translation was found for this node.
      return false;
    }

    this.translator.cancelSingleTranslation(translationId);

    if (!isNodeDetached(node)) {
      const element = /** @type {HTMLElement} */ (asHTMLElement(node));
      if (element) {
        const dataset = getDataset(element);
        if (dataset) {
          delete dataset.mozTranslationsId;
        }
        for (const childNode of element.querySelectorAll(
          "[data-moz-translations-id]"
        )) {
          delete childNode.dataset.mozTranslationsId;
        }
      }
    }

    this.#pendingContentTranslations.delete(node);
    this.#processedContentNodes.delete(node);

    return true;
  }

  /**
   * Attempts to cancel all attribute translations for the given node, even if the
   * relevant translation requests have already been sent to the TranslationsEngine.
   *
   * This function is primarily used by the mutation observer, when we are certain
   * that content has changed, and the previous translation is no longer valid.
   *
   * @param {Node} node
   * @returns {boolean}
   *   - True if any pending attribute translations were found for this node.
   */
  #preventAttributeTranslations(node) {
    const element = asElement(node);
    if (!element) {
      // We only translate attributes on Element type nodes.
      return false;
    }

    const attributes = this.#pendingAttributeTranslations.get(element);
    if (!attributes) {
      // No pending attribute translations were found for this element.
      return false;
    }

    for (const translationId of attributes.values()) {
      this.translator.cancelSingleTranslation(translationId);
    }
    this.#pendingAttributeTranslations.delete(element);

    return true;
  }

  /**
   * Queues a node's relevant attributes to be translated if it has any attributes that are
   * determined to be translatable, and if the node itself has not been excluded from translations.
   *
   * Otherwise does nothing with the node.
   *
   * @param {Element} element - The node for which to maybe translate attributes.
   */
  #maybeEnqueueElementForAttributeTranslation(element) {
    const translatableAttributes = this.#getTranslatableAttributes(element);

    if (translatableAttributes) {
      this.#enqueueElementForAttributeTranslation(
        element,
        translatableAttributes
      );
    }
  }

  /**
   * Queues a node to translate any attributes in the given attributeSet.
   *
   * This function translates the attributes in the given attributeSet without
   * restriction and should only be used if the list has already been validated
   * that the node has these attributes and that they are deemed translatable.
   *
   * If you do not already have a valid list of translatable attributes, then you
   * should use the maybeQueueNodeForAttributeTranslation method instead.
   *
   * @see #maybeEnqueueElementForAttributeTranslation
   *
   * @param {Element} element - The node for which to translate attributes.
   * @param {Set<string>} attributeSet - A set of pre-validated, translatable attributes.
   */
  #enqueueElementForAttributeTranslation(element, attributeSet) {
    /** @type {NodeVisibility} */
    let visibility = "out-of-viewport";
    if (isNodeHidden(element)) {
      visibility = "hidden";
    } else if (isNodeInViewport(element)) {
      visibility = "in-viewport";
    }
    this.#queuedAttributeElements.set(element, { attributeSet, visibility });
  }

  /**
   * Retrieves an array of translatable attributes within the given node.
   *
   * If the node is deemed to be excluded from translation, no attributes
   * will be returned even if they are otherwise translatable.
   *
   * @see TRANSLATABLE_ATTRIBUTES
   * @see TranslationsDocument.contentExcludedNodeSelector
   *
   * @param {Node} node - The node from which to retrieve translatable attributes.
   *
   * @returns {null | Set<string>} - The translatable attribute names from the given node.
   */
  #getTranslatableAttributes(node) {
    const element = asHTMLElement(node);
    if (!element) {
      // We only translate attributes on element node types.
      return null;
    }

    if (element.closest(this.attributeExcludedNodeSelector)) {
      // Either this node or an ancestor is explicitly excluded from translations, so we should not translate.
      return null;
    }

    let attributes = null;

    for (const attribute of TRANSLATABLE_ATTRIBUTES.keys()) {
      if (isAttributeTranslatable(node, attribute)) {
        if (!attributes) {
          attributes = new Set();
        }
        attributes.add(attribute);
      }
    }

    return attributes;
  }

  /**
   * Start and stop translation as the page is shown. For instance, this will
   * transition into "hidden" when the user tabs away from a document.
   */
  #handleVisibilityChange = () => {
    if (this.#sourceDocument.visibilityState === "visible") {
      this.translator.showPage();
    } else {
      ChromeUtils.addProfilerMarker(
        "Translations",
        { innerWindowId: this.#innerWindowId },
        "Pausing translations and discarding the port"
      );
      this.translator.hidePage();
    }
  };

  /**
   * Remove any dangling event handlers.
   */
  destroy() {
    this.translator.destroy();
    this.#stopMutationObserver();
    this.#sourceDocument.removeEventListener(
      "visibilitychange",
      this.#handleVisibilityChange
    );
  }

  /**
   * Helper function for adding a new root to the mutation
   * observer.
   *
   * @param {Node} root
   */
  #observeNewRoot(root) {
    this.#rootNodes.add(root);
    this.#mutationObserver.observe(root, MUTATION_OBSERVER_OPTIONS);
  }

  /**
   * Shadow roots are used in custom elements, and are a method for encapsulating
   * markup. Normally only "open" shadow roots can be accessed, but in privileged
   * contexts, they can be traversed using the ChromeOnly property openOrClosedShadowRoot.
   *
   * @param {Node} node
   */
  #addShadowRootsToObserver(node) {
    const { ownerDocument } = node;
    if (!ownerDocument) {
      return;
    }
    const nodeIterator = ownerDocument.createTreeWalker(
      node,
      NodeFilter.SHOW_ELEMENT,
      currentNode =>
        getShadowRoot(currentNode)
          ? NodeFilter.FILTER_ACCEPT
          : NodeFilter.FILTER_SKIP
    );

    /** @type {Node | null} */
    let currentNode;
    while ((currentNode = nodeIterator.nextNode())) {
      // Only shadow hosts are accepted nodes
      const shadowRoot = ensureExists(getShadowRoot(currentNode));
      if (!this.#rootNodes.has(shadowRoot)) {
        this.#observeNewRoot(shadowRoot);
      }
      // A shadow root may contain other shadow roots, recurse into them.
      this.#addShadowRootsToObserver(shadowRoot);
    }
  }

  /**
   * Add a new element to start translating. This root is tracked for mutations and
   * kept up to date with translations. This will be the body element and title tag
   * for the document.
   *
   * @param {Node | null | undefined} node
   */
  #addRootElement(node) {
    if (!node) {
      return;
    }

    const element = asHTMLElement(node);
    if (!element) {
      return;
    }

    if (this.#rootNodes.has(element)) {
      // Exclude nodes that are already targeted.
      return;
    }

    this.#rootNodes.add(element);

    if (element.nodeName === "TITLE") {
      // The <title> node is special, in that it will never intersect with the viewport,
      // so we must explicitly enqueue it for translation here.
      this.#enqueueNodeForContentTranslation(element);
      this.#maybeEnqueueElementForAttributeTranslation(element);
      return;
    }

    if (element.nodeName !== "HEAD") {
      // We do not consider the <head> element for content translations, only attributes.
      const contentStartTime = Cu.now();
      this.#subdivideNodeForContentTranslations(element);
      ChromeUtils.addProfilerMarker(
        "TranslationsDocument Add Root",
        { startTime: contentStartTime, innerWindowId: this.#innerWindowId },
        `Subdivided new root "${node.nodeName}" for content translations`
      );
    }

    this.#subdivideNodeForAttributeTranslations(element);

    this.#mutationObserver.observe(element, MUTATION_OBSERVER_OPTIONS);
    this.#addShadowRootsToObserver(element);
  }

  /**
   * Add qualified nodes to queueNodeForTranslation by recursively walk
   * through the DOM tree of node, including elements in Shadow DOM.
   *
   * @param {Node} node
   */
  #processSubdivide(node) {
    const { ownerDocument } = node;
    if (!ownerDocument) {
      return;
    }

    // This iterator will contain each node that has been subdivided enough to be translated.
    const nodeIterator = ownerDocument.createTreeWalker(
      node,
      NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT,
      this.#determineTranslationStatusForUnprocessedNodes
    );

    let currentNode;
    while ((currentNode = nodeIterator.nextNode())) {
      const shadowRoot = getShadowRoot(currentNode);
      if (shadowRoot) {
        this.#processSubdivide(shadowRoot);
      } else {
        this.#enqueueNodeForContentTranslation(currentNode);
      }
    }
  }

  /**
   * Start walking down through a node's subtree and decide which nodes to queue for
   * content translation. This first node could be the root nodes of the DOM, such as
   * the document body, or the title element, or it could be a mutation target.
   *
   * The nodes go through a process of subdivision until an appropriate sized chunk
   * of inline text can be found.
   *
   * @param {Node} node
   *
   * @returns {null | Array<Promise<unknown>>}
   */
  #subdivideNodeForContentTranslations(node) {
    if (!this.#rootNodes.has(node)) {
      // This is a non-root node, which means it came from a mutation observer.
      // This new node could be a host element for shadow tree
      const shadowRoot = getShadowRoot(node);
      if (shadowRoot && !this.#rootNodes.has(shadowRoot)) {
        this.#observeNewRoot(shadowRoot);
      } else {
        // Ensure that it is a valid node to translate by checking all of its ancestors.
        for (let parent of getAncestorsIterator(node)) {
          // Parent is ShadowRoot. We can stop here since this is
          // the top ancestor of the shadow tree.
          if (parent.containingShadowRoot == parent) {
            break;
          }
          if (
            this.#determineTranslationStatus(parent) ===
            NodeStatus.NOT_TRANSLATABLE
          ) {
            return null;
          }
        }
      }
    }

    switch (this.#determineTranslationStatusForUnprocessedNodes(node)) {
      case NodeStatus.NOT_TRANSLATABLE: {
        // This node is rejected as it shouldn't be translated.
        return null;
      }

      // SHADOW_HOST and READY_TO_TRANSLATE both map to FILTER_ACCEPT
      case NodeStatus.SHADOW_HOST:
      case NodeStatus.READY_TO_TRANSLATE: {
        const shadowRoot = getShadowRoot(node);
        if (shadowRoot) {
          this.#processSubdivide(shadowRoot);
        } else {
          // This node is ready for translating, and doesn't need to be subdivided. There
          // is no reason to run the TreeWalker, it can be directly submitted for
          // translation.
          this.#enqueueNodeForContentTranslation(node);
        }
        break;
      }

      case NodeStatus.SUBDIVIDE_FURTHER: {
        // This node may be translatable, but it needs to be subdivided into smaller
        // pieces. Create a TreeWalker to walk the subtree, and find the subtrees/nodes
        // that contain enough inline elements to send to be translated.
        this.#processSubdivide(node);
        break;
      }
    }

    return this.#dispatchQueuedTranslations();
  }

  /**
   * Uses query selectors to locate all of the elements that have translatable attributes,
   * then registers those elements with the intersection observers for their attributes
   * to be translated when observed.
   *
   * @param {Node} node
   *
   * @returns {Array<Promise<void>> | null}
   */
  #subdivideNodeForAttributeTranslations(node) {
    const element = asElement(node);
    if (!element) {
      // We only translate attributes on Element type nodes.
      return null;
    }
    this.#maybeEnqueueElementForAttributeTranslation(element);

    const childElementsWithTranslatableAttributes = element.querySelectorAll(
      TRANSLATABLE_ATTRIBUTES_SELECTOR
    );

    for (const childElement of childElementsWithTranslatableAttributes) {
      this.#maybeEnqueueElementForAttributeTranslation(childElement);
    }

    return this.#dispatchQueuedAttributeTranslations();
  }

  /**
   * Test whether this is an element we do not want to translate. These are things like
   * <code> elements, elements with a different "lang" attribute, and elements that
   * have a `translate=no` attribute.
   *
   * @param {Node} node
   */
  #isExcludedNode(node) {
    // Property access be expensive, so destructure required properties so they are
    // not accessed multiple times.
    const { nodeType } = node;

    if (nodeType === Node.TEXT_NODE) {
      // Text nodes are never excluded.
      return false;
    }
    const element = asElement(node);
    if (!element) {
      // Only elements and and text nodes should be considered.
      return true;
    }

    const { nodeName } = element;

    if (CONTENT_EXCLUDED_TAGS.has(nodeName.toUpperCase())) {
      // SVG tags can be lowercased, so ensure everything is uppercased.
      // This is an excluded tag.
      return true;
    }

    if (!this.#matchesDocumentLanguage(element)) {
      // Exclude nodes that don't match the sourceLanguage.
      return true;
    }

    if (element.getAttribute("translate") === "no") {
      // This element has a translate="no" attribute.
      // https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/translate
      return true;
    }

    if (element.classList.contains("notranslate")) {
      // Google Translate skips translations if the classList contains "notranslate"
      // https://cloud.google.com/translate/troubleshooting
      return true;
    }

    if (asHTMLElement(element)?.isContentEditable) {
      // This field is editable, and so exclude it similar to the way that form input
      // fields are excluded.
      return true;
    }

    return false;
  }

  /**
   * Runs `determineTranslationStatus`, but only on unprocessed nodes.
   *
   * @param {Node} node
   *
   * @returns {number} - One of the NodeStatus values.
   */
  #determineTranslationStatusForUnprocessedNodes = node => {
    if (this.#processedContentNodes.has(node)) {
      // Skip nodes that have already been processed.
      return NodeStatus.NOT_TRANSLATABLE;
    }

    return this.#determineTranslationStatus(node);
  };

  /**
   * Determines if a node should be submitted for translation, not translatable, or if
   * it should be subdivided further. It doesn't check if the node has already been
   * processed.
   *
   * The return result works as a TreeWalker NodeFilter as well.
   *
   * @param {Node} node
   *
   * @returns {number} - One of the `NodeStatus` values. See that object
   *   for documentation. These values match the filters for the TreeWalker.
   *   These values also work as a `NodeFilter` value.
   */
  #determineTranslationStatus(node) {
    if (getShadowRoot(node)) {
      return NodeStatus.SHADOW_HOST;
    }

    if (isNodeOrParentIncluded(node, this.#queuedContentNodes)) {
      // This node or its parent was already queued for translation: reject it.
      return NodeStatus.NOT_TRANSLATABLE;
    }

    if (this.#isExcludedNode(node)) {
      // This is an explicitly excluded node.
      return NodeStatus.NOT_TRANSLATABLE;
    }

    if (!node.textContent?.trim().length) {
      // Do not use subtrees that are empty of text. This textContent call is fairly
      // expensive.
      return !node.hasChildNodes()
        ? NodeStatus.NOT_TRANSLATABLE
        : NodeStatus.SUBDIVIDE_FURTHER;
    }

    if (nodeNeedsSubdividing(node)) {
      // Skip this node, and dig deeper into its tree to cut off smaller pieces
      // to translate. It is presumed to be a wrapper of block elements.
      return NodeStatus.SUBDIVIDE_FURTHER;
    }

    if (
      containsExcludedNode(node, this.contentExcludedNodeSelector) &&
      !hasNonWhitespaceTextNodes(node)
    ) {
      // Skip this node, and dig deeper into its tree to cut off smaller pieces
      // to translate.
      return NodeStatus.SUBDIVIDE_FURTHER;
    }

    // This node can be treated as entire block to submit for translation.
    return NodeStatus.READY_TO_TRANSLATE;
  }

  /**
   * Attempts to enqueue the given node for content translations.
   *
   * @param {Node} node
   */
  #enqueueNodeForContentTranslation(node) {
    /** @type {NodeVisibility} */
    let visibility = "out-of-viewport";
    if (isNodeHidden(node)) {
      visibility = "hidden";
    } else if (isNodeInViewport(node)) {
      visibility = "in-viewport";
    }

    if (!this.#processedContentNodes.has(node)) {
      this.#queuedContentNodes.set(node, visibility);
    }
  }

  /**
   * Submit each translatable attribute for the given element to translations engine
   * to have the attribute text translated.
   *
   * @param {Element} element
   * @param {Set<string>} attributeSet
   *
   * @returns {Promise<unknown>}
   */
  #submitForAttributeTranslation(element, attributeSet) {
    const promises = [];

    for (const attribute of attributeSet) {
      const sourceText = element.getAttribute(attribute);

      if (!sourceText?.trim().length) {
        continue;
      }
      const translationId = this.#lastTranslationId++;

      let pendingAttributes = this.#pendingAttributeTranslations.get(element);
      if (!pendingAttributes) {
        pendingAttributes = new Map();
        this.#pendingAttributeTranslations.set(element, pendingAttributes);
      }
      pendingAttributes.set(attribute, translationId);

      promises.push(
        this.#tryTranslate(
          element,
          sourceText,
          false /*isHTML*/,
          translationId
        ).then(
          translation => {
            if (
              translation &&
              this.#validateAttributeResponse(
                element,
                attribute,
                translationId,
                translation,
                false /* removeAttribute */
              )
            ) {
              this.#registerElementForAttributeTranslationUpdate(
                element,
                translation,
                attribute,
                translationId
              );
            }
          },
          error => {
            lazy.console.error(error);
          }
        )
      );
    }

    return Promise.allSettled(promises);
  }

  /**
   * Schedule a node to be updated with a translation.
   *
   * @param {Element} element
   * @param {string} translation
   * @param {string} attribute
   * @param {number} translationId
   */
  #registerElementForAttributeTranslationUpdate(
    element,
    translation,
    attribute,
    translationId
  ) {
    // Add the nodes to be populated with the next translation update.
    this.#elementsWithTranslatedAttributes.add({
      element,
      translation,
      attribute,
      translationId,
    });

    if (this.#pendingContentTranslationsCount === 0) {
      // No translations are pending, update the node.
      this.#updateElementsWithAttributeTranslations();
    } else if (!this.#hasPendingUpdateAttributesCallback) {
      // Schedule an update.
      this.#hasPendingUpdateAttributesCallback = lazy.setTimeout(
        this.#updateElementsWithAttributeTranslations.bind(this),
        DOM_UPDATE_INTERVAL_MS
      );
    } else {
      // An update has been previously scheduled, do nothing here.
    }
  }

  /**
   * Updates all elements that have completed attribute translation requests.
   *
   * This function is called asynchronously, so nodes may already be dead. Before
   * accessing a node make sure and run `Cu.isDeadWrapper` to check that it is alive.
   */
  #updateElementsWithAttributeTranslations() {
    // Stop the mutations so that the updates won't trigger observations.

    this.#pauseMutationObserverAndThen(() => {
      for (const entry of this.#elementsWithTranslatedAttributes) {
        const { element, translation, attribute, translationId } = entry;

        if (
          this.#validateAttributeResponse(
            element,
            attribute,
            translationId,
            translation,
            true /* removeAttribute */
          )
        ) {
          // Update the attribute of the node with translated attribute
          element.setAttribute(attribute, translation);
        }
      }
      this.#elementsWithTranslatedAttributes.clear();
      this.#hasPendingUpdateAttributesCallback = null;
    });
  }

  /**
   * Submit a node for translation to the translations engine.
   *
   * @param {Node} node
   *
   * @returns {Promise<void>}
   */
  async #submitForContentTranslation(node) {
    // Give each element an id that gets passed through the translation so it can be
    // reunited later on.
    const element = asElement(node);
    if (element) {
      /** @type {Array<Element>} */
      const elements = element.querySelectorAll("*");

      elements.forEach((el, i) => {
        const dataset = getDataset(el);
        if (dataset) {
          dataset.mozTranslationsId = String(i);
        }
      });
    }

    /** @type {string} */
    let sourceText;
    /** @type {boolean} */
    let isHTML;

    if (
      // This must be a text node
      !element ||
      // When an element has no child elements and its textContent is exactly
      // equal to its innerHTML, then it is safe to treat as a text translation.
      (element.childElementCount === 0 &&
        element.textContent === element.innerHTML)
    ) {
      sourceText = node.textContent ?? "";
      isHTML = false;
    } else {
      sourceText = /** @type {string} */ (element.innerHTML);
      isHTML = true;
    }

    if (sourceText.trim().length === 0) {
      return;
    }

    const translationId = this.#lastTranslationId++;
    this.#pendingContentTranslations.set(node, translationId);
    this.#walkNodeToPendingParent(node);

    // Mark this node as not to be translated again unless the contents are changed
    // (which the observer will pick up on)
    this.#processedContentNodes.add(node);

    const translatedContent = await this.#tryTranslate(
      node,
      sourceText,
      isHTML,
      translationId
    );

    if (
      translatedContent &&
      this.#validateTranslationResponse(node, translationId, translatedContent)
    ) {
      this.#registerNodeForContentTranslationUpdate(
        node,
        translatedContent,
        translationId
      );
    }
  }

  /**
   * Walks the nodes to set the relationship between the node to the pending parent node.
   * This solves a performance problem with pages with large subtrees and lots of mutation.
   * For instance on YouTube it took 838ms to `getPendingNodeFromTarget` by going through
   * all pending translations. Caching this relationship reduced it to 26ms to walk it
   * while adding the pending translation.
   *
   * On a page like the Wikipedia "Cat" entry, there are not many mutations, and this
   * adds 4ms of additional wasted work.
   *
   * @param {Node} pendingParent
   */
  #walkNodeToPendingParent(pendingParent) {
    this.#nodeToPendingParent.set(pendingParent, pendingParent);
    const { ownerDocument } = pendingParent;
    if (!ownerDocument) {
      return;
    }
    const nodeIterator = ownerDocument.createTreeWalker(
      pendingParent,
      NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT
    );
    /** @type {Node | null} */
    let node;
    while ((node = nodeIterator.nextNode())) {
      this.#nodeToPendingParent.set(node, pendingParent);
    }
  }

  /**
   * Attempts to translate the given text for the given node.
   *
   * If we already have a cached result for this translation,
   * then we will resolve immediately and never send the request
   * to the TranslationsEngine.
   *
   * The request may also fail or be cancelled before it completes.
   *
   * @param {Node} node
   * @param {string} sourceText
   * @param {boolean} isHTML
   * @param {number} translationId
   *
   * @returns {Promise<string | null>}
   */
  async #tryTranslate(node, sourceText, isHTML, translationId) {
    try {
      /** @type {string | null | undefined} */
      let translation = this.#translationsCache.get(sourceText, isHTML);
      if (translation === undefined) {
        translation = await this.translator.translate(
          node,
          sourceText,
          isHTML,
          translationId
        );
        if (translation !== null) {
          this.#translationsCache.set(sourceText, translation, isHTML);
        }
      } else if (!this.#hasFirstVisibleChange) {
        this.#hasFirstVisibleChange = true;
        this.#actorReportFirstVisibleChange();
      }

      return translation;
    } catch (error) {
      lazy.console.log("Translation failed", error);
    }

    return null;
  }

  /**
   * Start the mutation observer, for instance after applying the translations to the DOM.
   */
  #startMutationObserver() {
    if (Cu.isDeadWrapper(this.#mutationObserver)) {
      // This observer is no longer alive.
      return;
    }

    for (const node of this.#rootNodes) {
      if (Cu.isDeadWrapper(node)) {
        // This node is no longer alive.
        continue;
      }
      this.#mutationObserver.observe(node, MUTATION_OBSERVER_OPTIONS);
    }
  }

  /**
   * Stop the mutation observer, for instance to apply the translations to the DOM.
   */
  #stopMutationObserver() {
    // Was the window already destroyed?
    if (!Cu.isDeadWrapper(this.#mutationObserver)) {
      this.#mutationObserver.disconnect();
    }
  }

  /**
   * Updates all nodes that have completed attribute translation requests.
   *
   * This function is called asynchronously, so nodes may already be dead. Before
   * accessing a node make sure and run `Cu.isDeadWrapper` to check that it is alive.
   */
  #updateNodesWithContentTranslations() {
    // Stop the mutations so that the updates won't trigger observations.
    this.#pauseMutationObserverAndThen(() => {
      const entries = this.#nodesWithTranslatedContent;
      for (const { node, translatedContent, translationId } of entries) {
        // Check if a mutation has submitted another translation for this node. If so,
        // discard the stale translation.
        if (this.#pendingContentTranslations.get(node) !== translationId) {
          continue;
        }

        if (Cu.isDeadWrapper(node)) {
          // The node is no longer alive.
          ChromeUtils.addProfilerMarker(
            "Translations",
            { innerWindowId: this.#innerWindowId },
            "Node is no long alive."
          );
          continue;
        }

        switch (node.nodeType) {
          case Node.TEXT_NODE: {
            if (translatedContent.trim().length !== 0) {
              // Only update the node if there is new text.
              node.textContent = translatedContent;
            }
            break;
          }
          case Node.ELEMENT_NODE: {
            const translationsDocument = this.#domParser.parseFromString(
              `<!DOCTYPE html><div>${translatedContent}</div>`,
              "text/html"
            );
            updateElement(translationsDocument, ensureExists(asElement(node)));

            break;
          }
        }

        this.#pendingContentTranslations.delete(node);
      }

      this.#nodesWithTranslatedContent.clear();
      this.#hasPendingUpdateContentCallback = null;
    });
  }

  /**
   * Stops the mutation observer while running the given callback,
   * then restarts the mutation observer once the callback has finished.
   *
   * @param {Function} callback - A callback to run while the mutation observer is paused.
   */
  #pauseMutationObserverAndThen(callback) {
    this.#stopMutationObserver();
    try {
      callback();
    } finally {
      this.#startMutationObserver();
    }
  }

  /**
   * Ensures that nodes with completed content translation requests will be updated.
   *
   * @param {Node} node
   * @param {string} translatedContent
   * @param {number} translationId - A unique id to identify this translation request.
   */
  #registerNodeForContentTranslationUpdate(
    node,
    translatedContent,
    translationId
  ) {
    // Add the nodes to be populated with the next translation update.
    this.#nodesWithTranslatedContent.add({
      node,
      translatedContent,
      translationId,
    });

    if (this.#pendingContentTranslationsCount === 0) {
      // No translations are pending, update the node.
      this.#updateNodesWithContentTranslations();
    } else if (!this.#hasPendingUpdateContentCallback) {
      // Schedule an update.
      this.#hasPendingUpdateContentCallback = lazy.setTimeout(
        this.#updateNodesWithContentTranslations.bind(this),
        DOM_UPDATE_INTERVAL_MS
      );
    } else {
      // An update has been previously scheduled, do nothing here.
    }
  }

  /**
   * Check to see if a language matches the document's source language.
   *
   * @param {Node} node
   */
  #matchesDocumentLanguage(node) {
    const lang = asHTMLElement(node)?.lang;
    if (!lang) {
      // No `lang` was present, so assume it matches the language.
      return true;
    }

    // First, cheaply check if language tags match, without canonicalizing.
    if (lazy.TranslationsUtils.langTagsMatch(this.#documentLanguage, lang)) {
      return true;
    }

    try {
      // Make sure the local is in the canonical form, and check again. This function
      // throws, so don't trust that the language tags are formatting correctly.
      const [language] = Intl.getCanonicalLocales(lang);

      return lazy.TranslationsUtils.langTagsMatch(
        this.#documentLanguage,
        language
      );
    } catch (_error) {
      return false;
    }
  }
}

/**
 * The AntiStarvationStack is a stack-like data structure with a predefined batch size.
 * Requests are pushed to the stack one at a time, but they may only be popped in a batch.
 *
 * The stack keeps track of whether the net count of requests has increased or decreased
 * between each time it pops a batch of request. If the size of the stack has not decreased
 * since the previous time a batch was popped, then it means that more requests are being
 * pushed to the stack than are being popped from the stack, and the stack is considered
 * to have starving requests.
 *
 * This terminology is derived from the idea that if the stack is growing faster than it is
 * processing, then requests at the bottom of the stack will never be popped, and they will starve,
 * i.e. they will never have a chance to be processed.
 *
 *  - https://en.wikipedia.org/wiki/Starvation_(computer_science)
 *
 * In order to ensure fairness in processing, when the stack has starving requests it will pull
 * a predefined portion of the batch from the bottom of the stack, instead of only from the top.
 * This ensures that if the stack is growing faster than it can be processed, we are guaranteed
 * to eventually process the oldest requests in the stack, given enough time, and no request will
 * ever starve entirely.
 *
 * It is recommended that the starvation batch portion is less than or equal half of the batch size.
 * This ensures that priority is still given to newer requests, as is the intent of the stack, while
 * still ensuring fairness in scheduling.
 *
 * The following is a diagram of several calls to popBatch(), demonstrating both normal calls to
 * popBatch() as well as calls to popBatch() under starvation conditions:
 *
 * AntiStarvationStack: size == 9, #batchSize == 5, #starvationBatchPortion == 2
 *
 *             ┌─┬─┬─┬─┬─┬─┬─┬─┬─┐
 *             └─┴─┴─┴─┴─┴─┴─┴─┴─┘
 * popBatch():         └────┬────┘
 *                          5
 *
 *             ┌─┬─┬─┬─┐
 *             └─┴─┴─┴─┘
 * push() x 7:         └──────┬──────┘
 *                            7
 *
 *             ┌─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐
 *             └─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┘
 * popBatch(): └─┬─┘           └──┬──┘
 *               2                3
 *
 *                 ┌─┬─┬─┬─┬─┬─┐
 *                 └─┴─┴─┴─┴─┴─┘
 * push() x 4:                 └───┬───┘
 *                                 4
 *
 *                 ┌─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐
 *                 └─┴─┴─┴─┴─┴─┴─┴─┴─┴─┘
 * popBatch():               └────┬────┘
 *                                5
 *
 *                 ┌─┬─┬─┬─┬─┐
 *                 └─┴─┴─┴─┴─┘
 */
class AntiStarvationStack {
  /**
   * The array that represents the internal stack.
   *
   * @type {Array<TranslationRequest>}
   */
  #stack = [];

  /**
   * Keeps track of the size of the stack the previous time a batch was popped.
   * This is used to determine if the stack contains any starving requests,
   * i.e. more requests are being pushed to the stack than are being popped.
   *
   * @type {number}
   */
  #sizeBeforePreviousPop = 0;

  /**
   * The size of the batch that will be popped from the top of stack when no
   * starvation is occurring, i.e. more requests are being popped than pushed.
   *
   * @type {number}
   */
  #batchSize = 2;

  /**
   * Returns the count of requests that are popped from this stack when calling popBatch().
   *
   * @see {AntiStarvationStack.popBatch}
   *
   * @returns {number}
   */
  get batchSize() {
    return this.#batchSize;
  }

  /**
   * The size of the batch that will be popped from the bottom of stack when the
   * stack has starving requests, i.e. more requests are being pushed than popped.
   *
   * When the stack is starving, then (#batchSize - #starvationBatchPortion)
   * nodes will still be removed from the top of the stack, but #starvationBatchPortion
   * nodes will also be removed from the bottom of the stack to ensure fairness for
   * continuing to process old requests in addition to new requests.
   *
   * @type {number}
   */
  #starvationBatchPortion = 1;

  /**
   * Constructs a new AntiStarvationStack.
   *
   * The given batchSize must be larger than the starvationBatchPortion.
   *
   * @param {number} batchSize
   * @param {number} starvationBatchPortion
   */
  constructor(batchSize, starvationBatchPortion) {
    this.#batchSize = batchSize;
    this.#starvationBatchPortion = starvationBatchPortion;

    if (this.#batchSize < 2) {
      throw new Error("Batch size must be at least 2.");
    }

    if (this.#starvationBatchPortion <= 0) {
      throw new Error("Starvation batch portion must be greater than zero.");
    }

    if (this.#batchSize < this.#starvationBatchPortion) {
      throw new Error(
        "Batch size must not be smaller than starvation batch portion."
      );
    }
  }

  /**
   * Returns the current count of requests in the stack.
   *
   * @returns {number}
   */
  get size() {
    return this.#stack.length;
  }

  /**
   * Pushes a translation request to the top of the stack.
   *
   * @param {TranslationRequest} request
   */
  push(request) {
    this.#stack.push(request);
  }

  /**
   * Pops at most #batchSize requests from the stack.
   *
   * If the stack is starving (i.e. the net count of requests in the stack has
   * increased since the previous call to popBatch(), rather than decreased),
   * then a portion of requests will be removed from the bottom of the stack
   * to ensure fairness in scheduling.
   *
   * @returns {{ starvationDetected: boolean, requests: Array<TranslationRequest>}}
   */
  popBatch() {
    const currentSize = this.size;
    const starvationDetected =
      // The stack was not empty the last time we popped.
      this.#sizeBeforePreviousPop > 0 &&
      // The net requests have not decreased since the last time we popped.
      currentSize >= this.#sizeBeforePreviousPop &&
      // The stack currently has more than one batch worth of requests.
      currentSize > this.#batchSize;

    this.#sizeBeforePreviousPop = currentSize;

    if (currentSize === 0) {
      return { starvationDetected, requests: [] };
    }

    let topBatchSize = this.#batchSize;
    let bottomBatchSize = 0;

    if (starvationDetected && currentSize > this.#batchSize) {
      // The stack is growing faster than it is being processed,
      // the stack contains more than one batch worth of requests.
      // We will pull some from the bottom and the top to prevent starvation.
      topBatchSize -= this.#starvationBatchPortion;
      bottomBatchSize = this.#starvationBatchPortion;
    }

    /** @type {Array<TranslationRequest>} */
    const requests = [];

    for (let i = 0; i < topBatchSize && this.size > 0; i++) {
      // @ts-ignore: this.#stack.pop() cannot return undefined here.
      requests.push(this.#stack.pop());
    }

    // Removing requests from the front of an array like this has O(n) performance characteristics.
    // An ideal solution here would utilize a deque with amortized O(1) popBack() and popFront()
    // guarantees. Unfortunately, JavaScript lacks a standard deque implementation at this time.
    //
    // We are operating on small arrays, usually single or double digits in size, low hundreds at most.
    // I have not found the performance characteristics here to be any sort of bottleneck; I rarely
    // see this function show up in performance profiles, even when translating high-activity live
    // stream comment sections, which is a prime scenario for starvation conditions.
    //
    // Until such a time that a deque is readily available in JavaScript, I do not feel the complexity
    // of writing a custom deque implementation is justified for our use case here.
    if (bottomBatchSize > 0) {
      const bottomPortion = this.#stack.slice(0, bottomBatchSize);
      requests.push(...bottomPortion);

      // Retain the rest of the stack without the bottom portion.
      this.#stack = this.#stack.slice(bottomBatchSize, this.size);
    }

    return { starvationDetected, requests };
  }

  /**
   * Removes a request from the stack if it matches the given translationId.
   *
   * @param {number} translationId
   * @returns {TranslationRequest | undefined}
   */
  remove(translationId) {
    const index = this.#stack.findIndex(
      request => translationId === request.translationId
    );

    if (index < 0) {
      // No request was found matching this translationId.
      // It may have already been sent to the TranslationsEngine.
      return undefined;
    }

    const request = this.#stack[index];

    // Removing requests from the middle of an array like this has O(n) performance characteristics.
    // An ideal solution here would utilize a table-based strategy with amortized O(1) removal guarantees.
    //
    // Unfortunately, using a table structure such as Map would make every call to popBatch() have O(n),
    // characteristics, even under non-starvation conditions, due to Map not having any double-ended
    // iteration capabilities at this time.
    //
    // We are operating on small arrays, usually single or double digits in size, low hundreds at most.
    // I have not found the performance characteristics here to be any sort of bottleneck; I rarely
    // see this function show up in performance profiles, even when scrolling rapidly through pages,
    // which is a prime scenario for cancelling requests and therefore removing them by their translationIds.
    this.#stack.splice(index, 1);

    return request;
  }

  /**
   * Clears all entries from the stack.
   */
  clear() {
    this.#stack = [];
  }
}

/**
 * The TranslationScheduler orchestrates when translation requests are sent to the TranslationsEngine.
 *
 * The scheduler implements a stack-based, newest-first priority-scheduling algorithm, which ensures
 * that the most recent content that enters proximity to the viewport, whether due to user scrolling,
 * or due to dynamic content entering the page, is translated at the highest priority.
 *
 * Although the scheduler ensures that the highest-priority requests are translated first, it also
 * ensures scheduling fairness with guarantees that every request will eventually be scheduled,
 * regardless of age or priority, even if more requests are coming in than can be processed.
 *
 * Fairness is guaranteed by the use of an anti-starvation stack @see {AntiStarvationStack}.
 *
 * Requests may be cancelled from the scheduler at any time, even after they are sent to the
 * TranslationsEngine, though the earlier a request is cancelled, the cheaper it is to do so.
 */
class TranslationScheduler {
  /**
   * The priorities of the translation requests, where P0 is the highest and P7 is the lowest.
   *
   * The priorities are determined by the TranslationsDocument, and are dynamically assigned
   * based on several factors including whether the request is for a content or an attribute
   * translation, the location of the element with respect to the viewport, and the user's
   * recent scrolling activity on the page.
   */
  static get P0() {
    return 0;
  }
  static get P1() {
    return 1;
  }
  static get P2() {
    return 2;
  }
  static get P3() {
    return 3;
  }
  static get P4() {
    return 4;
  }
  static get P5() {
    return 5;
  }
  static get P6() {
    return 6;
  }
  static get P7() {
    return 7;
  }

  /**
   * The count of active requests must be lower than this threshold before we will allow
   * sending any more requests to the TranslationsEngine.
   *
   * We want to strike a balance between being optimally reactive to changes that may
   * change request priorities, such as the user scrolling, while also sending a constant
   * flow of requests to the TranslationsEngine, minimizing CPU downtime in the worker between
   * finishing the current batch of requests and beginning to process the next batch of requests.
   *
   * This number may need to be increased if the performance of the TranslationsEngine worker
   * improves considerably, or if we ever have more than one worker translating in parallel.
   *
   * @type {number}
   */
  static get ACTIVE_REQUEST_THRESHOLD() {
    return 1;
  }

  /**
   * The port that sends translation requests to the TranslationsEngine.
   *
   * @type {MessagePort | null}
   */
  #port = null;

  /**
   * If a new port is needed, this callback will be invoked to request one
   * from the actor. After the actor obtains it, it calls `acquirePort`.
   *
   * @type {() => void}
   */
  #actorRequestNewPort;

  /**
   * A map from the translationId to its corresponding TranslationRequest.
   *
   * This map contains only the requests that have been sent to the TranslationsEngine.
   * Once the engine sends a translation response, we will match the translationId here
   * to resolve or reject the request's promise, then remove it from the map.
   *
   * This map is mutually exclusive to the #unscheduledRequestsPriorityMap.
   *
   * @type {Map<number, TranslationRequest>}
   */
  #activeRequests = new Map();

  /**
   * A map from the translationId to the corresponding request's priority.
   *
   * This map contains only the requests that have not yet been sent to the TranslationsEngine.
   * We use this map to look up which priority stack a request should be removed from if the
   * request needs to be cancelled.
   *
   * Once the scheduler send the request to the TranslationsEngine, the entry for the translationId
   * will be removed from this map, and an entry for the same id will be added to #activeRequests.
   *
   * @type {Map<number, number>}
   */
  #unscheduledRequestPriorities = new Map();

  /**
   * The stacks that correspond to the eight priorities a translation request can be assigned.
   * The lower the number, the higher the priority. Each priority corresponds to an index in this array.
   *
   * @see {TranslationScheduler.P0}
   * @see {TranslationScheduler.P1}
   * @see {TranslationScheduler.P2}
   * @see {TranslationScheduler.P3}
   * @see {TranslationScheduler.P4}
   * @see {TranslationScheduler.P5}
   * @see {TranslationScheduler.P6}
   * @see {TranslationScheduler.P7}
   */
  #priorityStacks = [
    new AntiStarvationStack(2, 1), // p0 stack
    new AntiStarvationStack(2, 1), // p1 stack
    new AntiStarvationStack(2, 1), // p2 stack
    new AntiStarvationStack(2, 1), // p3 stack
    new AntiStarvationStack(2, 1), // p4 stack
    new AntiStarvationStack(2, 1), // p5 stack
    new AntiStarvationStack(2, 1), // p6 stack
    new AntiStarvationStack(2, 1), // p7 stack
  ];

  #maxRequestsPerScheduleEvent = (() => {
    let requestCount = 0;

    for (const stack of this.#priorityStacks) {
      requestCount += stack.batchSize;
    }

    return requestCount;
  })();

  /**
   * Tracks the status of the translation engine.
   *
   * @type {EngineStatus}
   */
  #engineStatus = "uninitialized";

  /**
   * Read-only getter to retrieve the engine status.
   *
   * @returns {EngineStatus}
   */
  get engineStatus() {
    return this.#engineStatus;
  }

  /**
   * Whether the page is currently shown or not. If hidden, we pause processing
   * and do not attempt to send new translation requests to the engine.
   */
  #isPageShown = true;

  /**
   * If a port is being requested, we store a reference to that promise
   * (plus its resolve/reject) so that repeated requests are not re-sent.
   *
   * @type {{ promise: Promise<void>, resolve: Function, reject: Function } | null}
   */
  #portRequest = null;

  /**
   * Marks when we have a pending callback for scheduling more requests
   * This ensures that we won't over-schedule requests from multiple calls.
   *
   * @type {boolean}
   */
  #hasPendingScheduleRequestsCallback = false;

  /**
   * The InnerWindowID value to report to profiler markers.
   *
   * @type {number}
   */
  #innerWindowId;

  /**
   * A cache of translations that have already been computed.
   * This is cache is shared with the TranslationsDocument.
   *
   * @type {LRUCache}
   */
  #translationsCache;

  /**
   * Constructs a new TranslationScheduler.
   *
   * @param {MessagePort?} port - A port to send translation requests to the TranslationsEngine.
   * @param {number} innerWindowId - The innerWindowId for profiler markers.
   * @param {LRUCache} translationsCache - A cache of completed translations, shared with the TranslationsDocument.
   * @param {() => void} actorRequestNewPort - The function to call to ask the actor for a new port.
   */
  constructor(port, innerWindowId, translationsCache, actorRequestNewPort) {
    this.#innerWindowId = innerWindowId;
    this.#translationsCache = translationsCache;
    this.#actorRequestNewPort = actorRequestNewPort;

    if (port) {
      this.acquirePort(port);
    }
  }

  /**
   * @returns {boolean}
   */
  hasPendingScheduleRequestsCallback() {
    return this.#hasPendingScheduleRequestsCallback;
  }

  /**
   * Attaches an onmessage handler to manage any communication with the TranslationsEngine.
   * If we were waiting for a port (#portRequest), we resolve that once the engine indicates
   * "ready" or reject if it indicates failure.
   *
   * @see {TranslationsDocument.acquirePort}
   *
   * @param {MessagePort} port
   */
  acquirePort(port) {
    if (this.#port) {
      // If we already have a port open but we somehow got a new one,
      // discard the old and use the new. Typically not expected unless the engine
      // had an error or the page re-requested a new port forcibly.
      if (this.#engineStatus === "ready") {
        lazy.console.error(
          "Received a new translation port while one already existed."
        );
      }
      this.#discardPort();
    }

    this.#port = port;

    const portRequest = this.#portRequest;

    // Wire up message handling
    port.onmessage = event => {
      /** @type {{data: PortToPage}} */
      const { data } = /** @type {any} */ (event);

      switch (data.type) {
        case "TranslationsPort:TranslationResponse": {
          const { translationId, targetText } = data;
          const request = this.#activeRequests.get(translationId);

          if (request) {
            this.#activeRequests.delete(translationId);
            request.resolve(targetText);
          }

          break;
        }
        case "TranslationsPort:GetEngineStatusResponse": {
          if (portRequest) {
            const { resolve, reject } = portRequest;
            if (data.status === "ready") {
              resolve();
            } else {
              reject(new Error("The engine failed to load."));
            }
          }

          this.#engineStatus = data.status;

          if (data.status === "ready") {
            this.maybeScheduleMoreTranslationRequests();
          } else {
            for (const translationId of this.#activeRequests.keys()) {
              this.preventSingleTranslation(translationId);
            }

            for (const translationId of this.#unscheduledRequestPriorities.keys()) {
              this.preventUnscheduledTranslation(translationId);
            }
          }

          break;
        }
        case "TranslationsPort:EngineTerminated": {
          this.#discardPort();
          this.maybeScheduleMoreTranslationRequests();
          break;
        }
        default: {
          lazy.console.error("Unknown translations port message:", data);
          break;
        }
      }
    };

    // Ask for the engine status
    port.postMessage({ type: "TranslationsPort:GetEngineStatusRequest" });
  }

  /**
   * Returns a promise that will resolve when we have acquired a valid port.
   *
   * @returns {Promise<void>}
   */
  #getPortRequestPromise() {
    if (this.#portRequest) {
      // We already have a pending request to acquire a port.
      return this.#portRequest.promise;
    }

    if (this.#engineStatus === "ready") {
      // The engine is already ready for translating.
      return Promise.resolve();
    }

    if (this.#port) {
      // We already have a port: we don't need another one.
      return Promise.resolve();
    }

    const portRequest = Promise.withResolvers();
    this.#portRequest = portRequest;

    // Ask the actor for a new port (which eventually calls `acquirePort`).
    this.#actorRequestNewPort();

    this.#portRequest.promise
      .catch(error => {
        lazy.console.error(error);
      })
      .finally(() => {
        // If we haven't replaced #portRequest with another request,
        // clear it out now that it succeeded.
        if (portRequest === this.#portRequest) {
          this.#portRequest = null;
        }
      });

    return this.#portRequest.promise;
  }

  /**
   * Close the port and remove any chance of further messages to the TranslationsEngine.
   * Any active requests are moved back to the priority stacks from which they were scheduled.
   */
  #discardPort() {
    this.#preserveActiveRequests();

    if (this.#port) {
      this.#port.close();
      this.#port = null;
      this.#portRequest = null;
    }

    this.#engineStatus = "uninitialized";
  }

  /**
   * Called when the page becomes visible again, e.g. the user was on another tab
   * and switched back to this page as the active tab. Any requests that were left
   * in the stacks will resume to be scheduled.
   */
  async onShowPage() {
    this.#isPageShown = true;
    this.maybeScheduleMoreTranslationRequests();
  }

  /**
   * Called when the page is hidden, e.g. the user moved to a different tab.
   * Any active requests that had been sent to the TranslationsEngine will
   * be Cancelled and moved back to the corresponding priority stacks that
   * they came from.
   */
  async onHidePage() {
    this.#isPageShown = false;

    if (this.#portRequest) {
      //this.#portRequest.reject();
      // If the page is hidden while a port request is pending,
      // wait for that request to finish so we can move any in-flight
      // requests to the temp queue properly.
      try {
        await this.#portRequest.promise;
      } catch {
        // If the port request fails while hidden, not much to do.
      }

      if (this.#isPageShown) {
        // The page was re-shown while we were awaiting the pending port request.
        return;
      }
    }

    // Discard the port to avoid engine usage while hidden.
    this.#discardPort();
  }

  /**
   * Creates a new TranslationRequest, adds it to the stack that corresponds to its priority,
   * and returns a promise for the resolution or rejection of the request.
   *
   * @see {TranslationRequest}
   *
   * @param {Node} node - The node that corresponds to this translation request.
   * @param {string} sourceText - The source text to translate for this request.
   * @param {boolean} isHTML - True if the source text is HTML markup, false if it is plain text.
   * @param {number} translationId - The translationId that corresponds to this request.
   * @param {number} priority - The priority at which this request should be scheduled.
   * @returns {Promise<string | null>}
   *   The translated text, or null if the text is already translated, the request becomes stale, the translation fails.
   */
  createTranslationRequestPromise(
    node,
    sourceText,
    isHTML,
    translationId,
    priority
  ) {
    const { promise, resolve, reject } = Promise.withResolvers();
    this.#unscheduledRequestPriorities.set(translationId, priority);

    this.#priorityStacks[priority].push({
      node,
      sourceText,
      isHTML,
      translationId,
      priority,
      resolve,
      reject,
    });

    this.maybeScheduleMoreTranslationRequests();

    return promise;
  }

  /**
   * Attempts to cancel a translation request if it has not been sent to the TranslationsEngine.
   *
   * To fully cancel a request regardless of whether it has been scheduled or not,
   * use the `cancelSingleTranslation` method.
   *
   * @see {TranslationScheduler.preventSingleTranslation}
   *
   * @param {number} translationId - The translationId of the request to cancel.
   * @returns {boolean} - True if the request was Cancelled, otherwise false.
   */
  preventUnscheduledTranslation(translationId) {
    const priority = this.#unscheduledRequestPriorities.get(translationId);

    if (priority === undefined) {
      // We were unable to retrieve an unscheduled priority for the given translationId.
      // This request has likely already been sent to the TranslationsEngine.
      return false;
    }

    const request = this.#priorityStacks[priority].remove(translationId);

    if (request) {
      request.resolve(null);
    }

    this.#unscheduledRequestPriorities.delete(translationId);

    ChromeUtils.addProfilerMarker(
      `TranslationScheduler Cancel P${priority}`,
      { innerWindowId: this.#innerWindowId },
      `Cancelled one unscheduled P${priority} translation.`
    );

    return true;
  }

  /**
   * Cancel a translation request regardless of whether it has been sent to the TranslationsEngine.
   *
   * For a more conservative method to only cancel a request that has not yet been scheduled,
   * use the `maybePreventUnscheduledTranslation` method.
   *
   * @see {TranslationScheduler.preventUnscheduledTranslation}
   *
   * @param {number} translationId - The translationId of the request to cancel.
   * @returns {{
   *  didPrevent: boolean,
   *  didCancelFromScheduler: boolean,
   *  didCancelFromEngine: boolean,
   * }}
   */
  preventSingleTranslation(translationId) {
    if (this.preventUnscheduledTranslation(translationId)) {
      // We successfully canceled this request before it was scheduled: nothing more to do.
      return {
        didPrevent: true,
        didCancelFromScheduler: true,
        didCancelFromEngine: false,
      };
    }

    const request = this.#activeRequests.get(translationId);

    if (!request) {
      // This translation completed before we got a chance to cancel it.
      return {
        didPrevent: false,
        didCancelFromScheduler: false,
        didCancelFromEngine: false,
      };
    }

    // If the request is active, then it has been sent to the TranslationsEngine,
    // so we must attempt to send a cancel request to the engine as well.
    this.#port?.postMessage({
      type: "TranslationsPort:CancelSingleTranslation",
      translationId,
    });

    request.resolve(null);
    this.#activeRequests.delete(translationId);

    ChromeUtils.addProfilerMarker(
      `TranslationScheduler Cancel P${request.priority}`,
      { innerWindowId: this.#innerWindowId },
      `Cancelled one active P${request.priority} translation.`
    );

    // We may have cancelled the only active request, which may not receive a response now.
    // If so, we need to ensure that we continue to schedule more requests.
    this.maybeScheduleMoreTranslationRequests();

    return {
      didPrevent: true,
      didCancelFromScheduler: true,
      didCancelFromEngine: true,
    };
  }

  /**
   * Returns any active translation request back to the priority stack from which they came.
   * Whenever the scheduler resumes scheduling, these requests may be already fulfilled,
   * resulting in a no-op, or they will be picked back up where they were left off.
   */
  #preserveActiveRequests() {
    lazy.console.log(
      `Pausing translations with ${this.#activeRequests.size} active translation requests.`
    );

    if (!this.#hasActiveTranslationRequests()) {
      // There are no active requests to unschedule: nothing more to do.
      return;
    }

    for (const request of this.#activeRequests.values()) {
      const { translationId, priority } = request;

      this.#priorityStacks[priority].push(request);
      this.#unscheduledRequestPriorities.set(translationId, priority);
    }

    this.#activeRequests.clear();
  }

  /**
   * Returns true if the scheduler has few enough quests that it is within the
   * final batches that it will schedule until more requests come in.
   *
   * @returns {boolean}
   */
  isWithinFinalBatches() {
    return (
      this.#maxRequestsPerScheduleEvent >=
      this.#pendingTranslationRequestCount()
    );
  }

  /**
   * Returns the count of pending translation requests, both active and unscheduled.
   *
   * @returns {number}
   */
  #pendingTranslationRequestCount() {
    return this.#activeRequests.size + this.#unscheduledRequestPriorities.size;
  }

  /**
   * Returns true if the scheduler has any requests have been sent to the TranslationsEngine,
   * and have not yet received a response, otherwise false.
   *
   * @returns {boolean}
   */
  #hasActiveTranslationRequests() {
    return this.#activeRequests.size > 0;
  }

  /**
   * Returns true if the scheduler has any requests that have not yet been sent to the TranslationsEngine,
   * and are waiting in a corresponding priority stack to be scheduled, otherwise false.
   *
   * @returns {boolean}
   */
  #hasUnscheduledTranslationRequests() {
    return this.#unscheduledRequestPriorities.size > 0;
  }

  /**
   * Returns true if the conditions are met to schedule more requests by sending them to the TranslationsEngine,
   * otherwise false if the scheduler should wait longer before sending more requests over the port.
   *
   * @returns {boolean}
   */
  #shouldScheduleMoreTranslationRequests() {
    if (!this.#isPageShown) {
      // We should not spend CPU time if the page is hidden.
      return false;
    }

    if (this.#portRequest) {
      // We are still waiting for a port: we will try again if a port is acquired.
      return false;
    }

    if (this.#port && this.#engineStatus === "uninitialized") {
      // We have acquired a port, but we are still waiting for an engine status message.
      // We will try again if the engine becomes ready.
      return false;
    }

    if (this.#hasPendingScheduleRequestsCallback) {
      // There is already a pending callback to schedule more requests.
      return false;
    }

    if (
      this.#activeRequests.size > TranslationScheduler.ACTIVE_REQUEST_THRESHOLD
    ) {
      // There are too many active requests to schedule any more right now.
      return false;
    }

    if (!this.#hasUnscheduledTranslationRequests()) {
      // There are no unscheduled requests to be sent to the TranslationsEngine.
      return false;
    }

    return true;
  }

  /**
   * Schedules another batch of requests by sending them to the TranslationsEngine,
   * only if it makes sense to do so.
   */
  maybeScheduleMoreTranslationRequests() {
    if (!this.#shouldScheduleMoreTranslationRequests()) {
      // The conditions are not currently right to schedule more requests.
      return;
    }

    this.#hasPendingScheduleRequestsCallback = true;

    lazy.setTimeout(() => {
      this.#getPortRequestPromise()
        .then(this.#scheduleMoreTranslationRequests)
        .catch(error => {
          lazy.console.error(error);
          this.#hasPendingScheduleRequestsCallback = false;
        });
    }, 0);
  }

  /**
   * Schedules a batch of requests from the given stack by sending them to the TranslationsEngine.
   *
   * @param {AntiStarvationStack} stack - The stack from which to schedule the batch of requests.
   * @returns {boolean} - Returns true if starvation was detected in this stack, otherwise false.
   */
  #scheduleBatchFromStack(stack) {
    const { starvationDetected, requests } = stack.popBatch();

    for (const request of requests) {
      this.#maybeScheduleTranslationRequest(request);
    }

    return starvationDetected;
  }

  /**
   * Schedules another batch of requests from the priority stacks by sending them to the TranslationsEngine.
   * How many requests are scheduled, and from which stacks, will depend on the current state of the stacks.
   *
   * This function is intentionally written as a lambda so that it can be passed as a
   * callback without the need to explicitly bind `this` to the function object.
   */
  #scheduleMoreTranslationRequests = () => {
    if (!this.#port) {
      // We lost our port between when this function was registered on the event loop, and when it was invoked.
      // The best we can do is possibly try again, if the conditions are still right.
      this.#hasPendingScheduleRequestsCallback = false;
      this.maybeScheduleMoreTranslationRequests();
      return;
    }

    let stackSizesAtStart = null;
    const activeRequestsAtStart = this.#activeRequests.size;
    const unscheduledRequestsAtStart = this.#unscheduledRequestPriorities.size;
    if (Services.profiler.IsActive() || lazy.console.shouldLog("Debug")) {
      // We need to preserve the sizes prior to scheduling only if we are adding profiler markers,
      // or if we are logging to console debug. Otherwise we shouldn't bother with these computations.
      stackSizesAtStart = this.#priorityStacks.map(stack => stack.size);
    }

    // Schedule only as many requests as we are required to in order to achieve starvation fairness,
    // starting with the highest-priority stack and moving toward the lower-priority stacks.
    for (const stack of this.#priorityStacks) {
      const starvationDetected = this.#scheduleBatchFromStack(stack);

      if (stack.size === 0) {
        // This stack is now empty, so we are clear to schedule more lower-priority requests.
        continue;
      }

      if (starvationDetected) {
        // This stack is starving (i.e. more requests are being added than are being scheduled),
        // so we must process a batch of lower-priority requests on this cycle in order to keep
        // the priority-scheduling algorithm fair, otherwise we could, in theory, only ever process
        // the current-level stack if new requests of the same priority continue to come in at a high rate.
        continue;
      }

      // We just scheduled a batch of requests from the highest-relevant-priority stack, and the count of requests
      // in that stack is decreasing. We should break here so as not to schedule any lower-priority requests before
      // we absolutely need to. The lower-priority requests may be justifiably cancelled before we get to them,
      // such as being re-prioritized or removed if the user scrolls around the page. In the event that they are
      // not cancelled, then they are guaranteed to be scheduled eventually, either due to starvation fairness,
      // or simply when it is their turn after processing all of the higher-priority requests first.
      break;
    }

    this.#maybeAddProfilerMarkersForStacks(stackSizesAtStart);
    this.#maybeLogStackDataToConsoleDebug(
      stackSizesAtStart,
      activeRequestsAtStart,
      unscheduledRequestsAtStart
    );

    this.#hasPendingScheduleRequestsCallback = false;
  };

  /**
   * If actively profiling, adds a marker for how many requests wre scheduled from each stack, if any.
   *
   * Normally, we would rely on `ChromeUtils.addProfilerMarker()` itself to no-op if not profiling,
   * however there are calculations and conditions for whether or not to post a marker, and scheduling
   * happens quite frequently, so it is best to not waste time with these calculations if not profiling.
   *
   * @param {Array<number>?} stackSizesAtStart – The size of each stack prior to the slice of scheduling that just occurred.
   */
  #maybeAddProfilerMarkersForStacks(stackSizesAtStart) {
    if (!stackSizesAtStart || !Services.profiler.IsActive()) {
      return;
    }

    for (let priority = 0; priority < stackSizesAtStart.length; ++priority) {
      const scheduledCount =
        stackSizesAtStart[priority] - this.#priorityStacks[priority].size;

      if (scheduledCount > 0) {
        ChromeUtils.addProfilerMarker(
          `TranslationScheduler Send P${priority}`,
          { innerWindowId: this.#innerWindowId },
          `Posted ${scheduledCount} P${priority} translation requests.`
        );
      }
    }
  }

  /**
   * If "Debug" is available, logs how many requests were scheduled from each stack on this scheduling pass, starting
   * with the highest-priority stack and logging through to the lowest-priority stack that scheduled any request.
   *
   * Normally, we would rely on `lazy.console.debug()` itself to no-op if "Debug" does not lie within the max log level,
   * however there are calculations and conditions related to formatting this log nicely in the console, and scheduling
   * happens quite frequently, so it is best to not waste time with these calculations if we will not log them at all.
   *
   * Example:
   *
   * "Scheduler(_1 | 422) [ __1, 165, 132, __1, 106, __1, __8, __8 ] => P0(__1), P1(__2)"
   *             ╻    ╻      ╻    ╻    ╻    ╻    ╻    ╻    ╻    ╻       ╻        ╻
   *             │    │      │    │    │    │    │    │    │    │       │        │
   *             │    │      │    │    │    │    │    │    │    │       │        2 P1 requests were scheduled in this batch.
   *             │    │      │    │    │    │    │    │    │    │       │
   *             │    │      │    │    │    │    │    │    │    │       1 P0 request was scheduled in this batch.
   *             │    │      │    │    │    │    │    │    │    │
   *             │    │      │    │    │    │    │    │    │    There are 8 P7 requests
   *             │    │      │    │    │    │    │    │    │
   *             │    │      │    │    │    │    │    │    There are 8 P6 requests.
   *             │    │      │    │    │    │    │    │
   *             │    │      │    │    │    │    │    There is 1 P5 request.
   *             │    │      │    │    │    │    │
   *             │    │      │    │    │    │    There are 106 P4 requests.
   *             │    │      │    │    │    │
   *             │    │      │    │    │    There is 1 P3 request.
   *             │    │      │    │    │
   *             │    │      │    │    There are 132 P2 requests.
   *             │    │      │    │
   *             │    │      │    There are 165 P1 requests.
   *             │    │      │
   *             │    │      There is 1 P0 request.
   *             │    │
   *             │    There are 422 pending requests.
   *             │
   *             There is 1 active request.
   *
   * @param {Array<number>?} stackSizesAtStart – The size of each stack prior to the slice of scheduling that just occurred.
   * @param {number} activeRequestsAtStart - The number of active requests that the TranslationsEngine was processing at the
   *                                         moment we scheduled more requests from the stacks.
   * @param {number} unscheduledRequestsAtStart - The number of unscheduled requests that the TranslationsEngine was processing
   *                                         at the moment we scheduled more requests from the stacks.
   */
  #maybeLogStackDataToConsoleDebug(
    stackSizesAtStart,
    activeRequestsAtStart,
    unscheduledRequestsAtStart
  ) {
    if (!stackSizesAtStart || !lazy.console.shouldLog("Debug")) {
      return;
    }

    // Find the deepest priority stack that scheduled any requests.
    let maxStackDepth;
    for (let depth = stackSizesAtStart.length - 1; depth >= 0; --depth) {
      if (this.#priorityStacks[depth].size < stackSizesAtStart[depth]) {
        maxStackDepth = depth;
        break;
      }
    }

    if (maxStackDepth === undefined) {
      // No requests were scheduled on this pass.
      return;
    }

    const padLength = Math.max(
      3,
      ...stackSizesAtStart.map(n => String(n).length)
    );

    const segments = [];
    for (let priority = 0; priority <= maxStackDepth; ++priority) {
      const sizeAtStart = stackSizesAtStart[priority];
      const currentSize = this.#priorityStacks[priority].size;
      const scheduledCount = sizeAtStart - currentSize;

      const formatted =
        scheduledCount === 0
          ? "_".repeat(padLength)
          : String(scheduledCount).padStart(padLength, "_");

      segments.push(`P${priority}(${formatted})`);
    }

    const activeRequestsPadLength = String(
      this.#maxRequestsPerScheduleEvent
    ).length;
    const activeRequestsString =
      activeRequestsAtStart === 0
        ? "_".repeat(activeRequestsPadLength)
        : String(activeRequestsAtStart).padStart(activeRequestsPadLength, "_");

    const unscheduledRequestsString = String(
      unscheduledRequestsAtStart
    ).padStart(3, "_");

    lazy.console.debug(
      `Scheduler(${activeRequestsString} | ${unscheduledRequestsString}) ` +
        TranslationScheduler.#formatSizesAtStart(stackSizesAtStart) +
        ` => ${segments.join(", ")}`
    );
  }

  /**
   * Formats the sizes of each priority stack into a string that is nice to look
   * at in the JS console.
   *
   * Example:
   *
   * "[ __1, 165, 132, __1, 106, __1, __8, __8 ]"
   * //  P0   P1   P2   P3   P4   P5   P6   P7
   *
   * @param {Array<number>} stackSizesAtStart
   */
  static #formatSizesAtStart(stackSizesAtStart) {
    const padLength = Math.max(
      3,
      ...stackSizesAtStart.map(n => String(Math.abs(n)).length)
    );

    const segments = stackSizesAtStart.map(n =>
      n === 0 ? "_".repeat(padLength) : String(n).padStart(padLength, "_")
    );

    return `[ ${segments.join(", ")} ]`;
  }

  /**
   * Schedules the translation request by sending it to the TranslationsEngine only
   * if the node that is relevant to the request is not detached.
   *
   * @param {TranslationRequest} request
   */
  #maybeScheduleTranslationRequest(request) {
    const { node } = request;

    if (isNodeDetached(node)) {
      // If the node is dead, there is no need to schedule it.
      const { translationId, resolve } = request;

      this.#unscheduledRequestPriorities.delete(translationId);
      resolve(null);

      return;
    }

    this.#scheduleTranslationRequest(request);
  }

  /**
   * Schedules a translation request by sending it to the TranslationsEngine,
   * marking the request as active.
   *
   * @param {TranslationRequest} request
   */
  #scheduleTranslationRequest(request) {
    if (!this.#port) {
      // This should never happen, since we should only be scheduling requests under
      // circumstances in which we are certain that we have a valid port.
      lazy.console.error(
        "Attempt to schedule a translation request without a port."
      );

      // If this should ever happen, the best thing we can do to recover is to put
      // the request back onto its corresponding priority stack to be scheduled again.
      const { priority } = request;
      this.#priorityStacks[priority].push(request);

      return;
    }

    const { translationId, sourceText, isHTML } = request;

    this.#activeRequests.set(translationId, request);
    this.#unscheduledRequestPriorities.delete(translationId);

    if (this.#translationsCache.isAlreadyTranslated(sourceText, isHTML)) {
      // Our cache indicates that the text that is being sent to translate is an exact
      // match to the translated output text of a previous request. When this happens
      // we should simply signal to the engine that this is a no-op, rather than
      // attempting to re-translate text that is already in the target language.
      //
      // This can happen in cases where a website removes already-translated content,
      // and then puts it back in the same spot, triggering our mutation observers.
      //
      // Wikipedia does this, for example, with the "title" attributes on hyperlinks
      // nearly every time they are moused over.
      this.#port.postMessage({
        type: "TranslationsPort:Passthrough",
        translationId,
      });
      return;
    }

    const cachedTranslation = this.#translationsCache.get(sourceText, isHTML);
    if (cachedTranslation) {
      // We already have a matching translated output for this source text, but
      // it was not hot in the cache when this request was sent to the translator,
      // otherwise the TranslationsDocument would have handled it directly.
      //
      // This may happen when several nodes with identical text get queued for translation
      // all at the same time, while the cache was still cold, such as translating a nested
      // comment section with multiple collapsed expandable threads that say "2 replies".
      //
      // We will signal to the engine to simply pass the cached translation along as
      // the response instead of wasting CPU time trying to recompute the translation.
      this.#port.postMessage({
        type: "TranslationsPort:CachedTranslation",
        translationId,
        cachedTranslation,
      });
      return;
    }

    this.#port.postMessage({
      type: "TranslationsPort:TranslationRequest",
      translationId,
      sourceText,
      isHTML,
    });
  }

  /**
   * Cleans up everything, closing the port and removing all translation request data.
   */
  destroy() {
    this.#port?.close();
    this.#port = null;
    this.#portRequest?.reject();
    this.#portRequest = null;
    this.#engineStatus = "uninitialized";

    this.#activeRequests.clear();
    this.#unscheduledRequestPriorities.clear();

    for (const stack of this.#priorityStacks) {
      stack.clear();
    }
  }
}

/**
 * Returns true if an HTML element is hidden based on factors such as collapsed state and
 * computed style, otherwise false.
 *
 * @param {HTMLElement} element
 * @returns {boolean}
 */
function isHTMLElementHidden(element) {
  // This is a cheap and easy check that will not compute style or force reflow.
  if (element.hidden) {
    // The element is explicitly hidden.
    return true;
  }

  // Handle open/closed <details> elements. This will also not compute style or force reflow.
  // https://developer.mozilla.org/en-US/docs/Web/HTML/Reference/Elements/details
  if (
    // The element is within a closed <details>
    element.closest("details:not([open])") &&
    // The element is not part of the <summary> of the <details>, which is always visible, even when closed.
    !element.closest("summary")
  ) {
    // The element is within a closed <details> and is not part of the <summary>, therefore it is not visible.
    return true;
  }

  // This forces reflow, which has a performance cost, but this is also what JQuery uses for its :hidden and :visible.
  // https://github.com/jquery/jquery/blob/bd6b453b7effa78b292812dbe218491624994526/src/css/hiddenVisibleSelectors.js#L1-L10
  if (
    !(
      element.offsetWidth ||
      element.offsetHeight ||
      element.getClientRects().length
    )
  ) {
    return true;
  }

  const { ownerGlobal } = element;
  if (!ownerGlobal) {
    // We cannot compute the style without ownerGlobal, so we will assume it is not visible.
    return true;
  }

  // This flushes the style, which is a performance cost.
  const style = ownerGlobal.getComputedStyle(element);
  if (!style) {
    // We were unable to compute the style, so we will assume it is not visible.
    return true;
  }

  // This is an issue with the DOM library generation.
  // @ts-expect-error Property 'display' does not exist on type 'CSSStyleDeclaration'.ts(2339)
  const { display, visibility, opacity } = style;

  return (
    display === "none" ||
    visibility === "hidden" ||
    visibility === "collapse" ||
    opacity === "0"
  );
}

/**
 * This function returns the correct element to determine the
 * style of node.
 *
 * @param {Node} node
 *
 * @returns {HTMLElement | null}
 */
function getHTMLElementForStyle(node) {
  const element = asHTMLElement(node);
  if (element) {
    return element;
  }

  if (node.parentElement) {
    return asHTMLElement(node.parentElement);
  }

  // For cases like text node where its parent is ShadowRoot,
  // we'd like to use flattenedTreeParentNode
  if (node.flattenedTreeParentNode) {
    return asHTMLElement(node.flattenedTreeParentNode);
  }

  // If the text node is not connected or doesn't have a frame.
  return null;
}

/**
 * This function runs when walking the DOM, which means it is a hot function. It runs
 * fairly fast even though it is computing the bounding box. This is all done in a tight
 * loop, and it is done on mutations. Care should be taken with reflows caused by
 * getBoundingClientRect, as this is a common performance issue.
 *
 * The following are the counts of how often this is run on a news site:
 *
 * Given:
 *  1573 DOM nodes
 *  504 Text nodes
 *  1069 Elements
 *
 * There were:
 *  209 calls to get this funcion.
 *
 * @param {Node} node
 *
 * @returns {boolean}
 */
function isNodeInViewport(node) {
  const window = node.ownerGlobal;
  const document = node.ownerDocument;
  if (!window || !document || !document.documentElement) {
    return false;
  }

  const element = getElementForStyle(node);
  if (!element) {
    throw new Error("Unable to find the Element to compute the style for node");
  }

  const rect = element.getBoundingClientRect();
  return (
    rect.top >= 0 &&
    rect.left >= 0 &&
    rect.bottom <=
      (window.innerHeight || document.documentElement.clientHeight) &&
    rect.right <= (window.innerWidth || document.documentElement.clientWidth)
  );
}

/**
 * Actually perform the update of the element with the translated node. This step
 * will detach all of the "live" nodes, and match them up in the correct order as provided
 * by the translations engine.
 *
 * @param {Document} translationsDocument
 * @param {Element} element
 *
 * @returns {void}
 */
function updateElement(translationsDocument, element) {
  // This text should have the same layout as the target, but it's not completely
  // guaranteed since the content page could change at any time, and the translation process is async.
  //
  // The document has the following structure:
  //
  // <html>
  //   <head>
  //   <body>{translated content}</body>
  // </html>

  const originalHTML = element.innerHTML;

  /**
   * The Set of translation IDs for nodes that have been cloned.
   *
   * @type {Set<string>}
   */
  const clonedNodes = new Set();

  // Guard against unintended changes to the "value" of <option> elements during
  // translation. This issue occurs because if an <option> element lacks an explicitly
  // set "value" attribute, then the default "value" will be taken from the text content
  // when requested.
  //
  // For example, <option>dog</option> might be translated to <option>perro</option>.
  // Without an explicit "value", the implicit "value" would change from "dog" to "perro",
  // and this can cause problems for submissions to queries etc.
  //
  // To prevent this, we ensure every translated <option> has an explicit "value"
  // attribute, either preserving the original "value" or assigning it from the original
  // text content. This results in <option>dog</option> being translated to
  // <option value="dog">perro</option>
  //
  // https://developer.mozilla.org/en-US/docs/Web/HTML/Element/option#value
  if (element.tagName === "OPTION") {
    element.setAttribute(
      "value",
      /** @type {HTMLOptionElement} */ (element).value
    );
  }
  for (const option of element.querySelectorAll("option")) {
    option.setAttribute("value", option.value);
  }

  /**
   * Build up a mapping of any element that has a "value" field that may change based
   * on translations. In the recursive "merge" function below, we can remove <option>
   * elements from <select> elements, which could cause the value attribute to change
   * as the option is removed. This will need to be restored.
   *
   * @type {Map<Node, string>}
   */
  const nodeValues = new Map();
  for (const select of element.querySelectorAll("select")) {
    nodeValues.set(select, select.value);
  }

  const firstChild = translationsDocument.body?.firstChild;
  if (firstChild) {
    merge(element, firstChild);
  }

  // Restore the <select> values.
  if (element.tagName === "SELECT") {
    /** @type {HTMLSelectElement} */ (element).value =
      nodeValues.get(element) ?? "";
  }
  for (const select of element.querySelectorAll("select")) {
    select.value = nodeValues.get(select);
  }

  /**
   * Merge the live tree with the translated tree by re-using elements from the live tree.
   *
   * @param {Element} liveTree
   * @param {Node} translatedTree
   */
  function merge(liveTree, translatedTree) {
    /** @type {Map<string, Element>} */
    const liveElementsById = new Map();

    /** @type {Array<Text>} */
    const liveTextNodes = [];

    // Remove all the nodes from the liveTree, and categorize them by Text node or
    // Element node.
    /** @type {Node | null} */
    let node;
    while ((node = liveTree.firstChild)) {
      // This is a ChildNode with the `remove` method.
      const childNode = /** @type {ChildNode} */ (
        /** @type {unknown} */ (node)
      );
      childNode.remove();

      const childElement = asElement(node);
      const childTextNode = asTextNode(node);
      const dataset = getDataset(childElement);
      if (childElement && dataset) {
        liveElementsById.set(dataset.mozTranslationsId, childElement);
      } else if (childTextNode) {
        liveTextNodes.push(childTextNode);
      }
    }

    // The translated tree dictates the order.

    /** @type {Node[]} */
    const translatedNodes = [];
    for (const childNode of translatedTree.childNodes) {
      if (childNode) {
        translatedNodes.push(childNode);
      }
    }

    for (
      let translatedIndex = 0;
      translatedIndex < translatedNodes.length;
      translatedIndex++
    ) {
      const translatedNode = ensureExists(translatedNodes[translatedIndex]);
      const translatedTextNode = asTextNode(translatedNode);
      const translatedElement = asElement(translatedNode);
      const dataset = getDataset(translatedElement);

      if (translatedTextNode) {
        // Copy the translated text to the original Text node and re-append it.
        let liveTextNode = liveTextNodes.shift();

        if (liveTextNode) {
          liveTextNode.data = translatedTextNode.data;
        } else {
          liveTextNode = translatedTextNode;
        }

        liveTree.appendChild(liveTextNode);
      } else if (dataset) {
        const liveElementId = dataset.mozTranslationsId;
        // Element nodes try to use the already existing DOM nodes.

        // Find the element in the live tree that matches the one in the translated tree.
        let liveElement = liveElementsById.get(liveElementId);

        if (!liveElement) {
          lazy.console.warn("Could not find a corresponding live element", {
            path: createNodePath(translatedNode, translationsDocument.body),
            liveElementId,
            liveElementsById,
            translatedNode,
          });
          continue;
        }

        // Has this element already been added to the list? Then duplicate it and re-add
        // it as a clone. The Translations Engine can sometimes duplicate HTML.
        if (liveElement.parentNode) {
          liveElement = ensureExists(
            asElement(liveElement.cloneNode(true /* deep clone */))
          );
          clonedNodes.add(liveElementId);
          lazy.console.warn(
            "Cloning a node because it was already inserted earlier",
            {
              path: createNodePath(translatedNode, translationsDocument.body),
              translatedNode,
              liveElement,
            }
          );
        }

        if (isNodeTextEmpty(translatedNode) && !isNodeTextEmpty(liveElement)) {
          // The translated node has no text, but the original node does have text, so we should investigate.
          //
          // Note that it is perfectly fine if both the translated node and original node do not have text.
          // This occurs when attributes are translated on the node, but no text content was translated.
          //
          // However, since we have a case where the original node has text and the translated node does not,
          // this scenario may be caused by one of two situations:
          //
          //   1) The element was duplicated by translation but then not given text
          //      content. This happens on Wikipedia articles for example.
          //
          //   2) The translator messed up and could not translate the text. This
          //      happens on YouTube in the language selector. In that case, having the
          //      original text is much better than no text at all.
          //
          // To make sure it is case 1) and not case 2), check whether this is the only occurrence.
          for (let i = 0; i < translatedNodes.length; i++) {
            if (translatedIndex === i) {
              // This is the current node, not a sibling.
              continue;
            }
            const sibling = translatedNodes[i];
            const siblingDataset = getDataset(asElement(sibling));
            if (
              // Only consider other element nodes.
              sibling.nodeType === Node.ELEMENT_NODE &&
              // If the sibling's mozTranslationsId matches, then use the sibling's
              // node instead.
              liveElementId === siblingDataset?.mozTranslationsId
            ) {
              // This is case 1 from above. Remove this element's original text nodes,
              // since a sibling text node now has all of the text nodes.
              removeTextNodes(liveElement);
            }
          }

          // Report this issue to the console.
          lazy.console.warn(
            "The translated element has no text even though the original did.",
            {
              path: createNodePath(translatedNode, translationsDocument.body),
              translatedNode,
              liveElement,
            }
          );
        } else if (!isNodeTextEmpty(liveElement)) {
          // There are still text nodes to find and update, recursively merge.
          merge(liveElement, translatedNode);
        }

        // Put the live node back in the live branch. But now t has been synced with the
        // translated text and order.
        liveTree.appendChild(liveElement);
      }
    }

    const unhandledElements = [...liveElementsById].filter(
      ([, liveElement]) => !liveElement.parentNode
    );

    for (node of liveTree.querySelectorAll("*")) {
      const dataset = getDataset(asElement(node));
      if (dataset) {
        // Clean-up the live element ids.
        delete dataset.mozTranslationsId;
      }
    }

    if (unhandledElements.length) {
      lazy.console.warn(
        `${createNodePath(
          translatedTree,
          translationsDocument.body
        )} Not all nodes unified`,
        {
          unhandledElements,
          clonedNodes,
          originalHTML,
          translatedContent: translationsDocument.body?.innerHTML,
          liveTree: liveTree.outerHTML,
          translatedTree: asElement(translatedTree)?.outerHTML,
        }
      );
    }
  }
}

/**
 * For debug purposes, compute a string path to an element.
 *
 * e.g. "div/div#header/p.bold.string/a"
 *
 * @param {Node} node
 * @param {HTMLElement | null} [root]
 *
 * @returns {string}
 */
function createNodePath(node, root) {
  let path = "";
  if (!node.ownerDocument) {
    return path;
  }
  if (root === null) {
    root = node.ownerDocument.body;
  }
  if (node.parentNode && node.parentNode !== root) {
    path = createNodePath(node.parentNode, root);
  }
  path += `/${node.nodeName}`;

  const element = asElement(node);
  if (element) {
    if (element.id) {
      path += `#${element.id}`;
    } else if (element.className) {
      for (const className of element.classList) {
        path += "." + className;
      }
    }
  }
  return path;
}

/**
 * Returns true if the content of this node's text is empty, otherwise false.
 *
 * @param {Node} node
 *
 * @returns {boolean}
 */
function isNodeTextEmpty(node) {
  const htmlElement = asHTMLElement(node);
  if (htmlElement) {
    return htmlElement.innerText.trim().length === 0;
  }
  if (node.nodeType === Node.TEXT_NODE && node.nodeValue) {
    return node.nodeValue.trim().length === 0;
  }
  return true;
}

/**
 * Recursively removes text nodes from the given element and all of its children.
 *
 * @param {Node} node
 */
function removeTextNodes(node) {
  for (const child of node.childNodes) {
    switch (child?.nodeType) {
      case Node.TEXT_NODE: {
        node.removeChild(child);
        break;
      }
      case Node.ELEMENT_NODE: {
        removeTextNodes(child);
        break;
      }
      default: {
        break;
      }
    }
  }
}

/**
 * Test whether any of the direct child text nodes of are non-whitespace text nodes.
 *
 * For example:
 *   - `<p>test</p>`: yes
 *   - `<p> </p>`: no
 *   - `<p><b>test</b></p>`: no
 *
 * @param {Node} node
 *
 * @returns {boolean}
 */
function hasNonWhitespaceTextNodes(node) {
  if (node.nodeType !== Node.ELEMENT_NODE) {
    // Only check element nodes.
    return false;
  }

  for (const child of node.childNodes) {
    const textNode = asTextNode(child);
    if (textNode) {
      if (!textNode.textContent?.trim()) {
        // This is just whitespace.
        continue;
      }
      // A text node with content was found.
      return true;
    }
  }

  // No text nodes were found.
  return false;
}

/**
 * Like `#isExcludedNode` but looks at the full subtree. Used to see whether
 * we can submit a subtree, or whether we should split it into smaller
 * branches first to try to exclude more of the non-translatable content.
 *
 * @param {Node} node
 * @param {string} excludedNodeSelector
 *
 * @returns {boolean}
 */
function containsExcludedNode(node, excludedNodeSelector) {
  return Boolean(asElement(node)?.querySelector(excludedNodeSelector));
}

/**
 *
 * Check if this node or its parent's node is already included in the given Map or Set.
 *
 * @param {any} node
 * @param { Set<any> | Map<any, any> } nodes
 *
 * @returns {boolean}
 */
function isNodeOrParentIncluded(node, nodes) {
  if (nodes.size === 0) {
    return false;
  }

  if (nodes.has(node)) {
    return true;
  }

  // If the immediate parent is the body, it is allowed.
  if (node.parentNode === node.ownerDocument?.body) {
    return false;
  }

  // Accessing the parentNode is expensive here according to performance profiling. This
  // is due to XrayWrappers. Minimize reading attributes by storing a reference to the
  // `parentNode` in a named variable, rather than re-accessing it.

  /** @type {Node | null} */
  let parentNode;
  let lastNode = node;
  while ((parentNode = lastNode.parentNode)) {
    if (nodes.has(parentNode)) {
      return true;
    }
    lastNode = parentNode;
  }

  return false;
}

/**
 * Reads the elements computed style and determines if the element is a block-like
 * element or not. Every element that lays out like a block should be sent in as one
 * cohesive unit to be translated.
 *
 * @param {Node} node
 *
 * @returns {boolean}
 */
function getIsBlockLike(node) {
  const element = asElement(node);
  if (!element) {
    return false;
  }

  const { ownerGlobal } = element;
  if (!ownerGlobal) {
    return false;
  }

  if (element.namespaceURI === "http://www.w3.org/2000/svg") {
    // SVG elements will report as inline, but there is no block layout in SVG.
    // Treat every SVG element as being block so that every node will be subdivided.
    return true;
  }

  /** @type {Record<string, string>} */
  // @ts-expect-error - This is a workaround for the CSSStyleDeclaration not being indexable.
  const style = ownerGlobal.getComputedStyle(element) ?? { display: null };

  return style.display !== "inline" && style.display !== "none";
}

/**
 * Determine if this element is an inline element or a block element. Inline elements
 * should be sent as a contiguous chunk of text, while block elements should be further
 * subdivided before sending them in for translation.
 *
 * @param {Node} node
 *
 * @returns {boolean}
 */
function nodeNeedsSubdividing(node) {
  const element = asElement(node);
  if (!element) {
    // Only elements need to be further subdivided.
    return false;
  }

  for (let childNode of element.childNodes) {
    if (!childNode) {
      continue;
    }
    switch (childNode.nodeType) {
      case Node.TEXT_NODE: {
        // Keep checking for more inline or text nodes.
        continue;
      }
      case Node.ELEMENT_NODE: {
        if (getIsBlockLike(childNode)) {
          // This node is a block node, so it needs further subdividing.
          return true;
        } else if (nodeNeedsSubdividing(childNode)) {
          // This non-block-like node may contain other block-like nodes.
          return true;
        }

        // Keep checking for more inline or text nodes.
        continue;
      }
      default: {
        return true;
      }
    }
  }
  return false;
}

/**
 * Returns an iterator of a node's ancestors.
 *
 * @param {Node} node
 *
 * @returns {Generator<Node>}
 */
function* getAncestorsIterator(node) {
  const document = node.ownerDocument;
  if (!document) {
    return;
  }
  for (
    let parent = node.parentNode;
    parent && parent !== document.documentElement;
    parent = parent.parentNode
  ) {
    yield parent;
  }
}

/**
 * Determines whether an attribute on a given element is translatable based on the specified
 * criteria for TRANSLATABLE_ATTRIBUTES.
 *
 * @see TRANSLATABLE_ATTRIBUTES
 *
 * @param {Node} node - The DOM node on which the attribute is being checked.
 * @param {string} attribute - The attribute name to check for translatability.
 *
 * @returns {boolean}
 */
function isAttributeTranslatable(node, attribute) {
  const element = asHTMLElement(node);
  if (!element) {
    return false;
  }

  if (!element.hasAttribute(attribute)) {
    // The element does not have this attribute, so there is nothing to translate.
    return false;
  }

  if (!TRANSLATABLE_ATTRIBUTES.has(attribute)) {
    // The attribute is not listed in our translatable attributes, so we will not translate it.
    return false;
  }

  const criteria = TRANSLATABLE_ATTRIBUTES.get(attribute);

  if (!criteria) {
    // There are no further criteria specified for this attribute, so we translate this attribute for all elements.
    return true;
  }

  // There are further criteria specified, so attempt to find a matching criterion for the given element.
  return criteria.some(({ tagName, conditions }) => {
    if (tagName !== element.tagName) {
      // The tagName does not match the given element. Try the next criterion.
      return false;
    }

    if (!conditions) {
      // The tagName matches and there are no further conditions, so we always translate this attribute for this element.
      return true;
    }

    // The tagName matches, but further conditions are specified. Attempt to find a matching condition.
    return Object.entries(conditions).some(([key, values]) =>
      values.some(value => element.getAttribute(key) === value)
    );
  });
}

/**
 * Returns true if the node is dead or detached from the DOM, otherwise false if the nod is still live.
 *
 * @param {Node} node
 *
 * @returns {boolean}
 */
function isNodeDetached(node) {
  return (
    // This node is out of the DOM and already garbage collected.
    Cu.isDeadWrapper(node) ||
    // The node is detached, but not yet garbage collected,
    // or it has been re-parented to a parent that itself is not connected.
    !node.isConnected ||
    // Normally you could just check `node.parentElement` to see if an element is
    // part of the DOM, but the Chrome-only flattenedTreeParentNode is used to include
    // Shadow DOM elements, which have a null parentElement.
    !node.flattenedTreeParentNode
  );
}

/**
 * Use TypeScript to determine if the Node is an Element.
 *
 * @param {Node | null} node
 *
 * @returns {Element | null}
 */
function asElement(node) {
  if (node?.nodeType === Node.ELEMENT_NODE) {
    return /** @type {HTMLElement} */ (node);
  }
  return null;
}

/**
 * Use TypeScript to determine if the Node is an Element.
 *
 * @param {Node | null} node
 *
 * @returns {Text | null}
 */
function asTextNode(node) {
  if (node?.nodeType === Node.TEXT_NODE) {
    return /** @type {Text} */ (node);
  }
  return null;
}

/**
 * Use TypeScript to determine if the Node is an HTMLElement.
 *
 * @param {Node | null} node
 *
 * @returns {HTMLElement | null}
 */
function asHTMLElement(node) {
  // This is a chrome-only function, and is the recommended function for chrome
  // contexts. The TranslationsDocument could be used in non-chrome contexts in the
  // future, so ensure that this doesn't break future implementations.
  //
  // See - https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/use-isInstance.html
  if (HTMLElement.isInstance) {
    if (HTMLElement.isInstance(node)) {
      return /** @type {HTMLElement} */ (node);
    }
  } else if (
    // eslint-disable-next-line mozilla/use-isInstance
    node instanceof HTMLElement
  ) {
    return /** @type {HTMLElement} */ (node);
  }
  return null;
}

/**
 * @template T
 * @param {T | null | undefined} item
 *
 * @returns {T}
 */
function ensureExists(item, message = "Item did not exist") {
  if (item === null || item === undefined) {
    throw new Error(message);
  }
  return item;
}

/**
 * Get the ShadowRoot from the chrome-only openOrClosedShadowRoot API.
 *
 * @param {Node} node
 *
 * @returns {ShadowRoot | null}
 */
function getShadowRoot(node) {
  return asElement(node)?.openOrClosedShadowRoot ?? null;
}

/**
 * Workaround the Gecko DOM TypeScript definition for dataset.
 *
 * @param {Element | null | undefined} element
 *
 * @returns {Record<string, string> | null}
 */
function getDataset(element) {
  // @ts-expect-error Type 'DOMStringMap' is not assignable to type 'Record<string, string>'.
  return element?.dataset ?? null;
}
