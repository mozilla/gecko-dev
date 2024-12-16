/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  TranslationsUtils:
    "chrome://global/content/translations/TranslationsUtils.sys.mjs",
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
 * @typedef {(message: string) => Promise<string>} TranslationFunction
 */

/**
 * Create a translation cache with a limit. It implements a "least recently used" strategy
 * to remove old translations. After `#cacheExpirationMS` the cache will be emptied.
 * This cache is owned statically by the TranslationsChild. This means that it will be
 * re-used on page reloads if the origin of the site does not change.
 */
export class LRUCache {
  /** @type {Map<string, string>} */
  #htmlCache = new Map();
  /** @type {Map<string, string>} */
  #textCache = new Map();
  /** @type {string} */
  #fromLanguage;
  /** @type {string} */
  #toLanguage;

  /**
   * This limit is used twice, once for Text translations, and once for HTML translations.
   */
  #cacheLimit = 5_000;

  /**
   * This cache will self-destruct after 10 minutes.
   */
  #cacheExpirationMS = 10 * 60_000;

  /**
   * @param {string} fromLanguage
   * @param {string} toLanguage
   */
  constructor(fromLanguage, toLanguage) {
    this.#fromLanguage = fromLanguage;
    this.#toLanguage = toLanguage;
  }

  /**
   * @param {boolean} isHTML
   * @returns {boolean}
   */
  #getCache(isHTML) {
    return isHTML ? this.#htmlCache : this.#textCache;
  }

  /**
   * Get a translation if it exists from the cache, and move it to the end of the cache
   * to keep it alive longer.
   *
   * @param {string} sourceString
   * @param {boolean} isHTML
   * @returns {string}
   */
  get(sourceString, isHTML) {
    const cache = this.#getCache(isHTML);
    const targetString = cache.get(sourceString);

    if (targetString === undefined) {
      return undefined;
    }

    // Maps are ordered, move this item to the end of the list so it will stay
    // alive longer.
    cache.delete(sourceString);
    cache.set(sourceString, targetString);

    this.keepAlive();

    return targetString;
  }

  /**
   * @param {string} sourceString
   * @param {string} targetString
   * @param {boolean} isHTML
   */
  set(sourceString, targetString, isHTML) {
    const cache = this.#getCache(isHTML);
    if (cache.size === this.#cacheLimit) {
      // If the cache is at the limit, get the least recently used translation and
      // remove it. This works since Maps have keys ordered by insertion order.
      const key = cache.keys().next().value;
      cache.delete(key);
    }
    cache.set(sourceString, targetString);
    this.keepAlive();
  }

  /**
   * @param {string} fromLanguage
   * @param {string} toLanguage
   */
  matches(fromLanguage, toLanguage) {
    return (
      this.#fromLanguage === fromLanguage && this.#toLanguage === toLanguage
    );
  }

  /**
   * @type {number}
   */
  #timeoutId = 0;

  #pendingKeepAlive = false;

  /**
   * Clear out the cache on a timer.
   */
  keepAlive() {
    if (this.#timeoutId) {
      lazy.clearTimeout(this.#timeoutId);
    }
    if (!this.#pendingKeepAlive) {
      // Rather than continuously creating new functions in a tight loop, only schedule
      // one keepAlive timeout on the next tick.
      this.#pendingKeepAlive = true;

      lazy.setTimeout(() => {
        this.#pendingKeepAlive = false;
        this.#timeoutId = lazy.setTimeout(() => {
          this.#htmlCache = new Map();
          this.#textCache = new Map();
        }, this.#cacheExpirationMS);
      }, 0);
    }
  }
}

/**
 * How often the DOM is updated with translations, in milliseconds.
 */
const DOM_UPDATE_INTERVAL_MS = 50;

/**
 * These tags are excluded from translation.
 */
const EXCLUDED_TAGS = new Set([
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
 * - "value" is translatable only for "INPUT" elements whose "type" attribute is "button", "reset", or "submit".
 *
 * @type {Map<string, Array<{ tagName: string, conditions?: Record<string, Array<string>> }> | null>}
 */
const TRANSLATABLE_ATTRIBUTES = new Map([
  [
    "alt",
    [{ tagName: "IMG" }, { tagName: "INPUT", conditions: { type: ["image"] } }],
  ],
  ["aria-brailledescription", null],
  ["aria-braillelabel", null],
  ["aria-description", null],
  ["aria-label", null],
  ["aria-placeholder", null],
  ["aria-roledescription", null],
  ["aria-valuetext", null],
  ["label", [{ tagName: "TRACK" }]],
  ["placeholder", null],
  ["title", null],
  [
    // We only want to translate value attributes for button-like <input> elements.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1919230#c10
    "value",
    [{ tagName: "INPUT", conditions: { type: ["button", "reset", "submit"] } }],
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
   * The BCP 47 language tag that is used on the page.
   *
    @type {string} */
  documentLanguage;

  /**
   * The timeout between the first translation received and the call to update the DOM
   * with translations.
   */
  #updateTimeout = null;
  #attributeUpdateTimeout = null;

  /**
   * The nodes that need translations. They are queued when the document tree is walked,
   * and then they are dispatched for translation based on their visibility. The viewport
   * nodes are given the highest priority.
   *
   * @type {Map<Node, NodeVisibility>}
   */
  #queuedNodes = new Map();

  /**
   * The nodes that need Attribute translations. They are queued when the document tree is walked,
   * and then they are dispatched for translation based on their visibility. The viewport
   * nodes are given the highest priority.
   *
   * @type  {Map<Node, { attributeList: string[], visibility: NodeVisibility }>}
   */
  #queuedAttributeNodes = new Map();

  /**
   * The count of how many pending translations have been sent to the translations
   * engine.
   */
  #pendingTranslationsCount = 0;

  /**
   * The list of nodes that need updating with the translated HTML. These are batched
   * into an update. The translationId is a monotonically increasing number that
   * represents a unique id for a translation. It guards against races where a node is
   * mutated before the translation is returned. The translation is asynchronously
   * canceled during a mutation, but it can still return a translation before it is
   * canceled.
   *
   * @type {Set<{ node: Node, translatedHTML: string, translationId: number }>}
   */
  #nodesWithTranslatedHTML = new Set();

  /**
   * The list of nodes that need updating with the translated Attribute HTML. These are batched
   * into an update.
   *
   * @type {Set<{ node: Node, translation: string, attribute: string, translationId: number }>}
   */
  #nodesWithTranslatedAttributes = new Set();

  /**
   * The set of nodes that have been subdivided and processed for translation. They
   * should not be submitted again unless their contents have been changed.
   *
   * @type {WeakSet<Node>}
   */
  #processedNodes = new WeakSet();

  /**
   * All root elements we're trying to translate. This should be the `document.body`
   * and the the `title` element.
   *
   * @type {Set<Node>}
   */
  #rootNodes = new Set();

  /**
   * Collect mutated nodes, and send them to be re-translated once
   * every requestAnimationFrame.
   *
   * @type {Set<Node>}
   */
  #mutatedNodes = new Set();

  /**
   * @type {Map<Node, Set<string>>}
   */
  #mutatedAttributes = new Map();

  /**
   * Mark when a requestAnimationFrame has been scheduled for updating the mutated nodes.
   */
  #isMutatedNodesRAFScheduled = false;

  /**
   * This promise gets resolved when the initial viewport translations are done.
   * This is a key user-visible performance metric. It represents what the user
   * actually sees.
   *
   * @type {Promise<void> | null}
   */
  viewportTranslated = null;

  isDestroyed = false;

  /**
   * This boolean indicates whether the first visible DOM translation change is about to occur.
   *
   * @type {boolean}
   */
  hasFirstVisibleChange = false;

  /**
   * A unique ID that guards against races between translations and mutations.
   *
   * @type {Map<Node, number>}
   */
  #pendingTranslations = new Map();

  /**
   * Cache a map of all child nodes to their pending parents. This lookup was slow
   * from profiling sites like YouTube with lots of mutations. Caching the relationship
   * speeds it up.
   *
   * @type {WeakMap<Node, Node>}
   */
  #nodeToPendingParent = new WeakMap();

  /**
   * A unique ID that guards against races between translations and mutations. The
   * Map<string, number> is a mapping of the node's attribute to the translation id.
   *
   * @type {Map<Node, Map<string, number>>}
   */
  #pendingAttributes = new Map();

  /**
   * Start with 1 so that it will never be falsey.
   */
  #lastTranslationId = 1;

  /**
   * Construct a new TranslationsDocument. It is tied to a specific Document and cannot
   * be re-used. The translation functions are injected since this class shouldn't
   * manage the life cycle of the translations engines.
   *
   * @param {Document} document
   * @param {string} documentLanguage - The BCP 47 tag of the source language.
   * @param {string} toLanguage - The BCP 47 tag of the destination language.
   * @param {number} innerWindowId - This is used for better profiler marker reporting.
   * @param {MessagePort} port - The port to the translations engine.
   * @param {() => void} requestNewPort - Used when an engine times out and a new
   *                                      translation request comes in.
   * @param {() => void} reportVisibleChange - Used to report to the actor that the first visible change
   *                                          for a translation is about to occur.
   * @param {number} translationsStart
   * @param {() => number} now
   * @param {LRUCache} translationsCache
   */
  constructor(
    document,
    documentLanguage,
    toLanguage,
    innerWindowId,
    port,
    requestNewPort,
    reportVisibleChange,
    translationsStart,
    now,
    translationsCache
  ) {
    /**
     * The language of the document. If elements are found that do not match this language,
     * then they are skipped.
     *
     * @type {string}
     */
    this.documentLanguage = documentLanguage;

    /** @type {QueuedTranslator} */
    this.translator = new QueuedTranslator(
      port,
      requestNewPort,
      reportVisibleChange
    );

    /** @type {number} */
    this.innerWindowId = innerWindowId;

    /** @type {DOMParser} */
    this.domParser = new document.ownerGlobal.DOMParser();

    /** @type {Document} */
    this.document = document;

    /** @type {LRUCache} */
    this.translationsCache = translationsCache;

    /** @type {() => void} */
    this.actorReportFirstVisibleChange = reportVisibleChange;

    /**
     * This selector runs to find child nodes that should be excluded. It should be
     * basically the same implementation of `isExcludedNode`, but as a selector.
     *
     * @type {string}
     */
    this.excludedNodeSelector = [
      // Use: [lang|=value] to match language codes.
      //
      // Per: https://developer.mozilla.org/en-US/docs/Web/CSS/Attribute_selectors
      //
      // The elements with an attribute name of attr whose value can be exactly
      // value or can begin with value immediately followed by a hyphen, - (U+002D).
      // It is often used for language subcode matches.
      `[lang]:not([lang|="${this.documentLanguage}"])`,
      `[translate=no]`,
      `.notranslate`,
      `[contenteditable="true"]`,
      `[contenteditable=""]`,
      [...EXCLUDED_TAGS].join(","),
    ].join(",");

    /**
     * Define the type of the MutationObserver for editor type hinting.
     *
     * @type {typeof MutationObserver}
     */
    const DocumentsMutationObserver = document.ownerGlobal.MutationObserver;

    this.observer = new DocumentsMutationObserver(mutationsList => {
      for (const mutation of mutationsList) {
        const pendingNode = this.getPendingNodeFromTarget(mutation.target);
        if (pendingNode) {
          const translationId = this.#pendingTranslations.get(pendingNode);
          if (translationId) {
            // The node was still pending to be translated, cancel it and re-submit.
            this.cancelTranslation(pendingNode, translationId);
            this.markNodeMutated(pendingNode);
            if (mutation.type === "childList") {
              // New nodes could have been added, make sure we can follow their shadow roots.
              this.document.ownerGlobal.requestAnimationFrame(() => {
                this.addShadowRootsToObserver(pendingNode);
              });
            }
            continue;
          }
        }
        switch (mutation.type) {
          case "childList":
            for (const addedNode of mutation.addedNodes) {
              this.addShadowRootsToObserver(addedNode);
              this.markNodeMutated(addedNode);
            }
            for (const removedNode of mutation.removedNodes) {
              const translationId = this.#pendingTranslations.get(removedNode);
              if (translationId) {
                this.cancelTranslation(removedNode, translationId);
              }
              this.cancelPendingAttributes(removedNode);
            }
            break;
          case "characterData":
            // The mutated node will implement the CharacterData interface. The only
            // node of this type that contains user-visible text is the `Text` node.
            // Ignore others such as the comment node.
            // https://developer.mozilla.org/en-US/docs/Web/API/CharacterData
            if (mutation.target.nodeType === Node.TEXT_NODE) {
              this.#processedNodes.delete(mutation.target);
              this.markNodeMutated(mutation.target);
            }
            break;
          case "attributes":
            this.markAttributeMutated(mutation.target, mutation.attributeName);
            break;
          default:
            break;
        }
      }
    });

    this.document.addEventListener(
      "visibilitychange",
      this.handleVisibilityChange
    );

    const addRootElements = () => {
      this.addRootElement(document.querySelector("title"));
      this.addRootElement(document.body, true /* reportWordsInViewport */);
    };

    if (document.body) {
      addRootElements();
    } else {
      // The TranslationsDocument was invoked before the DOM was ready, wait for
      // it to be loaded.
      document.addEventListener("DOMContentLoaded", addRootElements);
    }

    this.viewportTranslated?.then(() => {
      ChromeUtils.addProfilerMarker(
        "TranslationsChild",
        { innerWindowId, startTime: now() },
        "Viewport translations"
      );
      ChromeUtils.addProfilerMarker(
        "TranslationsChild",
        { innerWindowId, startTime: translationsStart },
        "Time to first translation"
      );
    });

    document.documentElement.lang = toLanguage;

    lazy.console.log(
      "Beginning to translate.",
      // The defaultView may not be there on tests.
      document.defaultView?.location.href
    );
  }

  /**
   * @param {Node} node
   */
  markNodeMutated(node) {
    this.#processedNodes.delete(node);
    this.#mutatedNodes.add(node);
    this.ensureMutatedNodesUpdateIsScheduled();
  }

  /**
   * @param {Node} node
   * @param {string} attributeName
   */
  markAttributeMutated(node, attributeName) {
    if (isAttributeTranslatable(node, attributeName)) {
      let attributes = this.#mutatedAttributes.get(node);
      if (!attributes) {
        attributes = new Set();
        this.#mutatedAttributes.set(node, attributes);
      }
      attributes.add(attributeName);
      this.ensureMutatedNodesUpdateIsScheduled();
    }
  }

  /**
   * Nodes can be mutated in a tight loop. To guard against the performance of
   * re-translating nodes too frequently, batch the translation on a
   * requestAnimationFrame.
   */
  ensureMutatedNodesUpdateIsScheduled() {
    if (
      !this.#isMutatedNodesRAFScheduled &&
      (this.#mutatedNodes.size || this.#queuedAttributeNodes)
    ) {
      this.#isMutatedNodesRAFScheduled = true;
      // Perform a double requestAnimationFrame to:
      //   1. Reduce the number of invalidation cycles of canceling intermediate translations.
      //   2. Do less work on the main thread when there are many mutations.
      this.document.ownerGlobal.requestAnimationFrame(() => {
        this.document.ownerGlobal.requestAnimationFrame(() => {
          this.#isMutatedNodesRAFScheduled = false;

          // Ensure the nodes are still alive.
          const liveNodes = [];
          for (const node of this.#mutatedNodes) {
            if (isNodeDetached(node)) {
              this.#mutatedNodes.delete(node);
            } else {
              liveNodes.push(node);
            }
          }

          // Remove any nodes that are contained in another node.
          for (let i = 0; i < liveNodes.length; i++) {
            const node = liveNodes[i];
            if (!this.#mutatedNodes.has(node)) {
              continue;
            }
            for (let j = i + 1; j < liveNodes.length; j++) {
              const otherNode = liveNodes[j];

              if (!this.#mutatedNodes.has(otherNode)) {
                continue;
              }

              if (node.contains(otherNode)) {
                this.#mutatedNodes.delete(otherNode);
              } else if (otherNode.contains(node)) {
                this.#mutatedNodes.delete(node);
                break;
              }
            }
          }

          for (const node of this.#mutatedNodes) {
            this.addShadowRootsToObserver(node);
            this.subdivideNodeForTranslations(node);
            this.translateAttributes(node);
          }
          this.#mutatedNodes.clear();

          for (const [node, attributes] of this.#mutatedAttributes.entries()) {
            this.#queueNodeForAttributeTranslation(node, attributes);
          }
          this.dispatchQueuedAttributeTranslations();
        });
      });
    }
  }

  /**
   * If a pending node contains or is the target node, return that pending node.
   *
   * @param {Node} target
   * @returns {Node | null}
   */
  getPendingNodeFromTarget(target) {
    return this.#nodeToPendingParent.get(target);
  }

  /**
   * @param {Node} node
   * @param {number} translationId
   */
  cancelTranslation(node, translationId) {
    this.translator.cancelSingleTranslation(translationId);
    if (!isNodeDetached(node) && node.nodeType === Node.ELEMENT_NODE) {
      delete node?.dataset.mozTranslationsId;
      for (const childNode of node.querySelectorAll(
        "[data-moz-translations-id]"
      )) {
        delete childNode.dataset.mozTranslationsId;
      }
    }
    this.#pendingTranslations.delete(node);
    this.#processedNodes.delete(node);
  }

  cancelPendingAttributes(node) {
    const attributes = this.#pendingAttributes.get(node);
    if (attributes) {
      for (const translationId of attributes.values()) {
        this.translator.cancelSingleTranslation(translationId);
      }
      this.#pendingAttributes.delete(node);
    }
  }

  /**
   * Queues a node's relevant attributes to be translated if it has any attributes that are
   * determined to be translatable, and if the node itself has not been excluded from translations.
   *
   * Otherwise does nothing with the node.
   *
   * @param {Node} node - The node for which to maybe translate attributes.
   */
  maybeQueueNodeForAttributeTranslation(node) {
    const translatableAttributes = this.getTranslatableAttributes(node);

    if (translatableAttributes) {
      this.#queueNodeForAttributeTranslation(node, translatableAttributes);
    }
  }

  /**
   * Queues a node to translate any attributes in the given attributeList.
   *
   * This function translates the attributes in the given attributeList without
   * restriction and should only be used if the list has already been validated
   * that the node has these attributes and that they are deemed translatable.
   *
   * If you do not already have a valid list of translatable attributes, then you
   * should use the maybeQueueNodeForAttributeTranslation method instead.
   *
   * @see maybeQueueNodeForAttributeTranslation
   *
   * @param {Node} node - The node for which to translate attributes.
   * @param {Array<string>} attributeList - A list of pre-validated, translatable attributes.
   */
  #queueNodeForAttributeTranslation(node, attributeList) {
    /** @type {NodeVisibility} */
    let visibility = "out-of-viewport";
    if (isNodeHidden(node)) {
      visibility = "hidden";
    } else if (isNodeInViewport(node)) {
      visibility = "in-viewport";
    }
    this.#queuedAttributeNodes.set(node, { attributeList, visibility });
  }

  /**
   * Retrieves an array of translatable attributes within the given node.
   *
   * If the node is deemed to be excluded from translation, no attributes
   * will be returned even if they are otherwise translatable.
   *
   * @see TRANSLATABLE_ATTRIBUTES
   * @see TranslationsDocument.excludedNodeSelector
   *
   * @param {Node} node - The node from which to retrieve translatable attributes.
   *
   * @returns {null | Array<string>} - The translatable attribute names from the given node.
   */
  getTranslatableAttributes(node) {
    if (node.nodeType !== Node.ELEMENT_NODE) {
      // We only translate attributes on element node types.
      return null;
    }

    if (node.closest(this.excludedNodeSelector)) {
      // Either this node or an ancestor is explicitly excluded from translations, so we should not translate.
      return null;
    }

    /** @type {null | Array<string>} */
    let attributes = null;

    for (const attribute of TRANSLATABLE_ATTRIBUTES.keys()) {
      if (isAttributeTranslatable(node, attribute)) {
        attributes ? attributes.push(attribute) : (attributes = [attribute]);
      }
    }

    return attributes;
  }

  /**
   * Start and stop the translator as the page is shown. For instance, this will
   * transition into "hidden" when the user tabs away from a document.
   */
  handleVisibilityChange = () => {
    if (this.document.visibilityState === "visible") {
      this.translator.showPage();
    } else {
      ChromeUtils.addProfilerMarker(
        "Translations",
        { innerWindowId: this.innerWindowId },
        "Pausing translations and discarding the port"
      );
      this.translator.hidePage();
    }
  };

  /**
   * Remove any dangling event handlers.
   */
  destroy() {
    this.isDestroyed = true;
    this.translator.destroy();
    this.stopMutationObserver();
    this.document.removeEventListener(
      "visibilitychange",
      this.handleVisibilityChange
    );
  }

  /**
   * Helper function for adding a new root to the mutation
   * observer.
   *
   * @param {Node} root
   */
  observeNewRoot(root) {
    this.#rootNodes.add(root);
    this.observer.observe(root, MUTATION_OBSERVER_OPTIONS);
  }

  /**
   * Shadow roots are used in custom elements, and are a method for encapsulating
   * markup. Normally only "open" shadow roots can be accessed, but in privileged
   * contexts, they can be traversed using the ChromeOnly property openOrClosedShadowRoot.
   *
   * @param {Node} node
   */
  addShadowRootsToObserver(node) {
    const nodeIterator = node.ownerDocument.createTreeWalker(
      node,
      NodeFilter.SHOW_ELEMENT,
      currentNode =>
        currentNode.openOrClosedShadowRoot
          ? NodeFilter.FILTER_ACCEPT
          : NodeFilter.FILTER_SKIP
    );

    /** @type {Node | null} */
    let currentNode;
    while ((currentNode = nodeIterator.nextNode())) {
      // Only shadow hosts are accepted nodes
      const shadowRoot = currentNode.openOrClosedShadowRoot;
      if (!this.#rootNodes.has(shadowRoot)) {
        this.observeNewRoot(shadowRoot);
      }
      // A shadow root may contain other shadow roots, recurse into them.
      this.addShadowRootsToObserver(shadowRoot);
    }
  }

  /**
   * Add a new element to start translating. This root is tracked for mutations and
   * kept up to date with translations. This will be the body element and title tag
   * for the document.
   *
   * @param {Element} [node]
   */
  addRootElement(node) {
    if (!node) {
      return;
    }

    if (node.nodeType !== Node.ELEMENT_NODE) {
      // This node is not an element, do not add it.
      return;
    }

    if (this.#rootNodes.has(node)) {
      // Exclude nodes that are already targeted.
      return;
    }

    this.#rootNodes.add(node);

    let viewportNodeTranslations = this.subdivideNodeForTranslations(node);
    let viewportAttributeTranslations = this.translateAttributes(node);

    if (!this.viewportTranslated) {
      this.viewportTranslated = Promise.allSettled([
        ...(viewportNodeTranslations ?? []),
        ...(viewportAttributeTranslations ?? []),
      ]);
    }

    this.observer.observe(node, MUTATION_OBSERVER_OPTIONS);
    this.addShadowRootsToObserver(node);
  }

  /**
   * Add qualified nodes to queueNodeForTranslation by recursively walk
   * through the DOM tree of node, including elements in Shadow DOM.
   *
   * @param {Element} [node]
   */
  processSubdivide(node) {
    const nodeIterator = node.ownerDocument.createTreeWalker(
      node,
      NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT,
      this.determineTranslationStatusForUnprocessedNodes
    );

    // This iterator will contain each node that has been subdivided enough to
    // be translated.
    let currentNode;
    while ((currentNode = nodeIterator.nextNode())) {
      const shadowRoot = currentNode.openOrClosedShadowRoot;
      if (shadowRoot) {
        this.processSubdivide(shadowRoot);
      } else {
        this.queueNodeForTranslation(currentNode);
      }
    }
  }

  /**
   * Start walking down through a node's subtree and decide which nodes to queue for
   * translation. This first node could be the root nodes of the DOM, such as the
   * document body, or the title element, or it could be a mutation target.
   *
   * The nodes go through a process of subdivision until an appropriate sized chunk
   * of inline text can be found.
   *
   * @param {Node} node
   */
  subdivideNodeForTranslations(node) {
    if (!this.#rootNodes.has(node)) {
      // This is a non-root node, which means it came from a mutation observer.
      // This new node could be a host element for shadow tree
      const shadowRoot = node.openOrClosedShadowRoot;
      if (shadowRoot && !this.#rootNodes.has(shadowRoot)) {
        this.observeNewRoot(shadowRoot);
      } else {
        // Ensure that it is a valid node to translate by checking all of its ancestors.
        for (let parent of getAncestorsIterator(node)) {
          // Parent is ShadowRoot. We can stop here since this is
          // the top ancestor of the shadow tree.
          if (parent.containingShadowRoot == parent) {
            break;
          }
          if (
            this.determineTranslationStatus(parent) ===
            NodeStatus.NOT_TRANSLATABLE
          ) {
            return;
          }
        }
      }
    }

    switch (this.determineTranslationStatusForUnprocessedNodes(node)) {
      case NodeStatus.NOT_TRANSLATABLE:
        // This node is rejected as it shouldn't be translated.
        return;

      // SHADOW_HOST and READY_TO_TRANSLATE both map to FILTER_ACCEPT
      case NodeStatus.SHADOW_HOST:
      case NodeStatus.READY_TO_TRANSLATE: {
        const shadowRoot = node.openOrClosedShadowRoot;
        if (shadowRoot) {
          this.processSubdivide(shadowRoot);
        } else {
          // This node is ready for translating, and doesn't need to be subdivided. There
          // is no reason to run the TreeWalker, it can be directly submitted for
          // translation.
          this.queueNodeForTranslation(node);
        }
        break;
      }

      case NodeStatus.SUBDIVIDE_FURTHER:
        // This node may be translatable, but it needs to be subdivided into smaller
        // pieces. Create a TreeWalker to walk the subtree, and find the subtrees/nodes
        // that contain enough inline elements to send to be translated.
        this.processSubdivide(node);
        break;
    }

    if (node.nodeName === "BODY") {
      this.reportWordsInViewport();
    }
    this.dispatchQueuedTranslations();
  }

  /**
   * Get all the nodes which have selected attributes
   * from the node/document and queue them.
   * Call the translate function on these nodes
   *
   * @param {Node} node
   * @returns {Array<Promise<void>> | null}
   */
  translateAttributes(node) {
    if (node.nodeType !== Node.ELEMENT_NODE) {
      // Only element nodes may have attributes.
      return null;
    }
    this.maybeQueueNodeForAttributeTranslation(node);

    const childNodesWithTranslatableAttributes = node.querySelectorAll(
      TRANSLATABLE_ATTRIBUTES_SELECTOR
    );

    for (const childNode of childNodesWithTranslatableAttributes) {
      this.maybeQueueNodeForAttributeTranslation(childNode);
    }

    return this.dispatchQueuedAttributeTranslations();
  }

  /**
   * Test whether this is an element we do not want to translate. These are things like
   * <code> elements, elements with a different "lang" attribute, and elements that
   * have a `translate=no` attribute.
   *
   * @param {Node} node
   */
  isExcludedNode(node) {
    // Property access be expensive, so destructure required properties so they are
    // not accessed multiple times.
    const { nodeType } = node;

    if (nodeType === Node.TEXT_NODE) {
      // Text nodes are never excluded.
      return false;
    }
    if (nodeType !== Node.ELEMENT_NODE) {
      // Only elements and and text nodes should be considered.
      return true;
    }

    const { nodeName } = node;

    if (
      EXCLUDED_TAGS.has(
        // SVG tags can be lowercased, so ensure everything is uppercased.
        nodeName.toUpperCase()
      )
    ) {
      // This is an excluded tag.
      return true;
    }

    if (!this.matchesDocumentLanguage(node)) {
      // Exclude nodes that don't match the fromLanguage.
      return true;
    }

    if (node.getAttribute("translate") === "no") {
      // This element has a translate="no" attribute.
      // https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/translate
      return true;
    }

    if (node.classList.contains("notranslate")) {
      // Google Translate skips translations if the classList contains "notranslate"
      // https://cloud.google.com/translate/troubleshooting
      return true;
    }

    if (node.isContentEditable) {
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
   * @returns {number} - One of the NodeStatus values.
   */
  determineTranslationStatusForUnprocessedNodes = node => {
    if (this.#processedNodes.has(node)) {
      // Skip nodes that have already been processed.
      return NodeStatus.NOT_TRANSLATABLE;
    }

    return this.determineTranslationStatus(node);
  };

  /**
   * Determines if a node should be submitted for translation, not translatable, or if
   * it should be subdivided further. It doesn't check if the node has already been
   * processed.
   *
   * The return result works as a TreeWalker NodeFilter as well.
   *
   * @param {Node} node
   * @returns {number} - One of the `NodeStatus` values. See that object
   *   for documentation. These values match the filters for the TreeWalker.
   *   These values also work as a `NodeFilter` value.
   */
  determineTranslationStatus(node) {
    if (node.openOrClosedShadowRoot) {
      return NodeStatus.SHADOW_HOST;
    }

    if (isNodeQueued(node, this.#queuedNodes)) {
      // This node or its parent was already queued, reject it.
      return NodeStatus.NOT_TRANSLATABLE;
    }

    if (this.isExcludedNode(node)) {
      // This is an explicitly excluded node.
      return NodeStatus.NOT_TRANSLATABLE;
    }

    if (node.textContent.trim().length === 0) {
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
      containsExcludedNode(node, this.excludedNodeSelector) &&
      !hasTextNodes(node)
    ) {
      // Skip this node, and dig deeper into its tree to cut off smaller pieces
      // to translate.
      return NodeStatus.SUBDIVIDE_FURTHER;
    }

    // This node can be treated as entire block to submit for translation.
    return NodeStatus.READY_TO_TRANSLATE;
  }

  /**
   * Queue a node for translation.
   *
   * @param {Node} node
   */
  queueNodeForTranslation(node) {
    /** @type {NodeVisibility} */
    let visibility = "out-of-viewport";
    if (isNodeHidden(node)) {
      visibility = "hidden";
    } else if (isNodeInViewport(node)) {
      visibility = "in-viewport";
    }

    this.#queuedNodes.set(node, visibility);
  }

  /**
   * Submit the translations giving priority to nodes in the viewport.
   *
   * @returns {Array<Promise<void>> | null}
   */
  dispatchQueuedTranslations() {
    let inViewportCounts = 0;
    let outOfViewportCounts = 0;
    let hiddenCounts = 0;

    let inViewportTranslations = null;
    if (!this.viewportTranslated) {
      inViewportTranslations = [];
    }

    for (const [node, visibility] of this.#queuedNodes) {
      if (visibility === "in-viewport") {
        inViewportCounts++;
        const promise = this.submitTranslation(node);
        if (inViewportTranslations) {
          inViewportTranslations.push(promise);
        }
      }
    }
    for (const [node, visibility] of this.#queuedNodes) {
      if (visibility === "out-of-viewport") {
        outOfViewportCounts++;
        this.submitTranslation(node);
      }
    }
    for (const [node, visibility] of this.#queuedNodes) {
      if (visibility === "hidden") {
        hiddenCounts++;
        this.submitTranslation(node);
      }
    }

    ChromeUtils.addProfilerMarker(
      "Translations",
      { innerWindowId: this.innerWindowId },
      `Translate ${this.#queuedNodes.size} nodes.\n\n` +
        `In viewport: ${inViewportCounts}\n` +
        `Out of viewport: ${outOfViewportCounts}\n` +
        `Hidden: ${hiddenCounts}\n`
    );

    this.#queuedNodes.clear();
    return inViewportTranslations;
  }

  /**
   * Submit the Attribute translations giving priority to nodes in the viewport.
   *
   * @returns {Array<Promise<void>> | null}
   */
  dispatchQueuedAttributeTranslations() {
    let inViewportCounts = 0;
    let outOfViewportCounts = 0;
    let hiddenCounts = 0;

    let inViewportTranslations = null;
    if (!this.viewportTranslated) {
      inViewportTranslations = [];
    }
    // Submit the nodes with attrbutes to be translated.
    for (const [node, { attributeList, visibility }] of this
      .#queuedAttributeNodes) {
      if (visibility === "in-viewport") {
        inViewportCounts++;
        const promise = this.submitAttributeTranslation(node, attributeList);
        if (inViewportTranslations) {
          inViewportTranslations.push(promise);
        }
      }
    }
    for (const [node, { attributeList, visibility }] of this
      .#queuedAttributeNodes) {
      if (visibility === "out-of-viewport") {
        outOfViewportCounts++;
        this.submitAttributeTranslation(node, attributeList);
      }
    }
    for (const [node, { attributeList, visibility }] of this
      .#queuedAttributeNodes) {
      if (visibility === "hidden") {
        hiddenCounts++;
        this.submitAttributeTranslation(node, attributeList);
      }
    }

    ChromeUtils.addProfilerMarker(
      "Attribute Translations",
      { innerWindowId: this.innerWindowId },
      `Attribute Translate ${this.#queuedAttributeNodes.size} nodes.\n\n` +
        `In viewport: ${inViewportCounts}\n` +
        `Out of viewport: ${outOfViewportCounts}\n` +
        `Hidden: ${hiddenCounts}\n`
    );

    this.#queuedAttributeNodes.clear();

    return inViewportTranslations;
  }

  /**
   * Submit a node for Attribute translation to the translations engine.
   *
   * @param {Node} node
   * @param {string[]} attributeList
   * @returns {Promise<void>}
   */
  async submitAttributeTranslation(node, attributeList) {
    if (node.nodeType === Node.ELEMENT_NODE) {
      for (const attribute of attributeList) {
        const text = node.getAttribute(attribute);

        if (text.trim().length === 0) {
          continue;
        }
        const translationId = this.#lastTranslationId++;

        let pendingAttributes = this.#pendingAttributes.get(node);
        if (!pendingAttributes) {
          pendingAttributes = new Map();
          this.#pendingAttributes.set(node, pendingAttributes);
        }
        pendingAttributes.set(attribute, translationId);

        const translation = await this.maybeTranslate(
          node,
          text,
          false /*isHTML*/,
          translationId
        );

        if (
          this.validateAttributeResponse(
            node,
            attribute,
            translationId,
            translation
          )
        ) {
          this.scheduleNodeUpdateWithTranslationAttribute(
            node,
            translation,
            attribute,
            translationId
          );
        }
      }
    }
  }

  /**
   * Schedule a node to be updated with a translation.
   *
   * @param {Node} node
   * @param {string} translation
   * @param {string} attribute
   * @param {number} translationId
   */
  scheduleNodeUpdateWithTranslationAttribute(
    node,
    translation,
    attribute,
    translationId
  ) {
    // Add the nodes to be populated with the next translation update.
    this.#nodesWithTranslatedAttributes.add({
      node,
      translation,
      attribute,
      translationId,
    });

    if (this.#pendingTranslationsCount === 0) {
      // No translations are pending, update the node.
      this.updateNodesWithTranslationsAttributes();
    } else if (!this.#attributeUpdateTimeout) {
      // Schedule an update.
      this.#attributeUpdateTimeout = lazy.setTimeout(
        this.updateNodesWithTranslationsAttributes.bind(this),
        DOM_UPDATE_INTERVAL_MS
      );
    } else {
      // An update has been previously scheduled, do nothing here.
    }
  }

  /**
   * This is called every `DOM_UPDATE_INTERVAL_MS` ms with translations
   * for attributes in the nodes.
   *
   * This function is called asynchronously, so nodes may already be dead. Before
   * accessing a node make sure and run `Cu.isDeadWrapper` to check that it is alive.
   */
  updateNodesWithTranslationsAttributes() {
    // Stop the mutations so that the updates won't trigger observations.

    this.pauseMutationObserverAndRun(() => {
      for (const entry of this.#nodesWithTranslatedAttributes) {
        const { node, translation, attribute, translationId } = entry;

        if (
          this.validateAttributeResponse(
            node,
            attribute,
            translationId,
            translation,
            true /* removeAttribute */
          )
        ) {
          // Update the attribute of the node with translated attribute
          node.setAttribute(attribute, translation);
        }
      }
      this.#nodesWithTranslatedAttributes.clear();
      this.#attributeUpdateTimeout = null;
    });
  }

  /**
   * Record how many words were in the viewport, as this is the most important
   * user-visible translation content.
   */
  reportWordsInViewport() {
    if (
      // This promise gets created for the first dispatchQueuedTranslations
      this.viewportTranslated ||
      this.#queuedNodes.size === 0
    ) {
      return;
    }

    // TODO(Bug 1814195) - Add telemetry.
    // TODO(Bug 1820618) - This whitespace regex will not work in CJK-like languages.
    // This requires a segmenter for a proper implementation.

    const whitespace = /\s+/;
    let wordCount = 0;
    for (const [node, visibility] of this.#queuedNodes) {
      if (visibility === "in-viewport") {
        wordCount += node.textContent.trim().split(whitespace).length;
      }
    }

    const message = wordCount + " words are in the viewport.";
    lazy.console.log(message);
    ChromeUtils.addProfilerMarker(
      "Translations",
      { innerWindowId: this.innerWindowId },
      message
    );
  }

  /**
   * Submit a node for translation to the translations engine.
   *
   * @param {Node} node
   * @returns {Promise<void>}
   */
  async submitTranslation(node) {
    // Give each element an id that gets passed through the translation so it can be
    // reunited later on.
    if (node.nodeType === Node.ELEMENT_NODE) {
      node.querySelectorAll("*").forEach((el, i) => {
        el.dataset.mozTranslationsId = i;
      });
    }

    /** @type {string} */
    let text;
    /** @type {boolean} */
    let isHTML;

    if (node.nodeType === Node.ELEMENT_NODE) {
      text = node.innerHTML;
      isHTML = true;
    } else {
      text = node.textContent;
      isHTML = false;
    }

    if (text.trim().length === 0) {
      return;
    }

    const translationId = this.#lastTranslationId++;
    this.#pendingTranslations.set(node, translationId);
    this.walkNodeToPendingParent(node);

    // Mark this node as not to be translated again unless the contents are changed
    // (which the observer will pick up on)
    this.#processedNodes.add(node);

    const translatedHTML = await this.maybeTranslate(
      node,
      text,
      isHTML,
      translationId
    );

    if (this.validateTranslationResponse(node, translationId, translatedHTML)) {
      this.scheduleNodeUpdateWithTranslation(
        node,
        translatedHTML,
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
  walkNodeToPendingParent(pendingParent) {
    this.#nodeToPendingParent.set(pendingParent, pendingParent);

    const nodeIterator = pendingParent.ownerDocument.createTreeWalker(
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
   * Handle stale responses, or null responses. Returns true when the translation
   * can be applied. This method has a side effect of cleaning up pending translations.
   *
   * @param {Node} node
   * @param {number} translationId
   * @param {string | null} translation
   * @returns {boolean}
   */
  validateTranslationResponse(node, translationId, translation) {
    if (isNodeDetached(node)) {
      return false;
    }
    if (this.#pendingTranslations.get(node) !== translationId) {
      // This translation lost a race, and was re-submitted under a
      // different translationId.
      return false;
    }

    if (translation == null) {
      // The translation had an error, remove it from the pending translations.
      this.#pendingTranslations.delete(node);
      return false;
    }

    return true;
  }

  /**
   * Handle stale responses, or null responses. Returns true when the translation
   * can be applied. This method has a side effect of cleaning up pending translations.
   *
   * @param {Node} node
   * @param {string} attribute
   * @param {number} translationId
   * @param {string | null} translation
   * @param {boolean} removeAttribute
   * @returns {boolean}
   */
  validateAttributeResponse(
    node,
    attribute,
    translationId,
    translation,
    removeAttribute
  ) {
    if (isNodeDetached(node)) {
      return false;
    }
    const pendingAttributes = this.#pendingAttributes.get(node);
    if (!pendingAttributes) {
      // The pending attribute was deleted.
      return false;
    }

    if (pendingAttributes.get(attribute) !== translationId) {
      // This translation lost a race, and was re-submitted under a
      // different translationId.
      return false;
    }

    if (translation == null) {
      // The translation had an error, remove it from the pending translations.
      pendingAttributes.delete(node);
      if (pendingAttributes.size === 0) {
        this.#pendingAttributes.delete(node);
      }
      return false;
    }

    if (removeAttribute) {
      pendingAttributes.delete(node);
      if (pendingAttributes.size === 0) {
        this.#pendingAttributes.delete(node);
      }
    }

    return true;
  }

  /**
   * A single function to update pendingTranslationsCount while
   * calling the translate function
   *
   * @param {Node} node
   * @param {string} text
   * @param {boolean} isHTML
   * @param {number} translationId
   * @returns {Promise<string | null>}
   */
  async maybeTranslate(node, text, isHTML, translationId) {
    this.#pendingTranslationsCount++;
    try {
      let translation = this.translationsCache.get(text, isHTML);
      if (translation === undefined) {
        translation = await this.translator.translate(
          node,
          text,
          isHTML,
          translationId
        );
        this.translationsCache.set(text, translation, isHTML);
      } else if (!this.hasFirstVisibleChange) {
        this.hasFirstVisibleChange = true;
        this.actorReportFirstVisibleChange();
      }
      return translation;
    } catch (error) {
      lazy.console.log("Translation failed", error);
    } finally {
      this.#pendingTranslationsCount--;
    }
    return null;
  }

  /**
   * Start the mutation observer, for instance after applying the translations to the DOM.
   */
  startMutationObserver() {
    if (Cu.isDeadWrapper(this.observer)) {
      // This observer is no longer alive.
      return;
    }
    for (const node of this.#rootNodes) {
      if (Cu.isDeadWrapper(node)) {
        // This node is no longer alive.
        continue;
      }
      this.observer.observe(node, MUTATION_OBSERVER_OPTIONS);
    }
  }

  /**
   * Stop the mutation observer, for instance to apply the translations to the DOM.
   */
  stopMutationObserver() {
    // Was the window already destroyed?
    if (!Cu.isDeadWrapper(this.observer)) {
      this.observer.disconnect();
    }
  }

  /**
   * This is called every `DOM_UPDATE_INTERVAL_MS` ms with translations for nodes.
   *
   * This function is called asynchronously, so nodes may already be dead. Before
   * accessing a node make sure and run `Cu.isDeadWrapper` to check that it is alive.
   */
  updateNodesWithTranslations() {
    // Stop the mutations so that the updates won't trigger observations.
    this.pauseMutationObserverAndRun(() => {
      const entry = this.#nodesWithTranslatedHTML;
      for (const { node, translatedHTML, translationId } of entry) {
        // Check if a mutation has submitted another translation for this node. If so,
        // discard the stale translation.
        if (this.#pendingTranslations.get(node) !== translationId) {
          continue;
        }

        if (Cu.isDeadWrapper(node)) {
          // The node is no longer alive.
          ChromeUtils.addProfilerMarker(
            "Translations",
            { innerWindowId: this.innerWindowId },
            "Node is no long alive."
          );
          continue;
        }

        switch (node.nodeType) {
          case Node.TEXT_NODE: {
            if (translatedHTML.trim().length !== 0) {
              // Only update the node if there is new text.
              node.textContent = translatedHTML;
            }
            break;
          }
          case Node.ELEMENT_NODE: {
            // TODO (Bug 1820625) - This is slow compared to the original implementation
            // in the addon which set the innerHTML directly. We can't set the innerHTML
            // here, but perhaps there is another way to get back some of the performance.
            const translationsDocument = this.domParser.parseFromString(
              `<!DOCTYPE html><div>${translatedHTML}</div>`,
              "text/html"
            );
            updateElement(translationsDocument, node);
            break;
          }
        }
        this.#pendingTranslations.delete(node);
      }

      this.#nodesWithTranslatedHTML.clear();
      this.#updateTimeout = null;
    });
  }

  /**
   * Stop the mutations so that the updates of the translations
   * in the nodes won't trigger observations.
   *
   * @param {Function} run The function to update translations
   */
  pauseMutationObserverAndRun(run) {
    this.stopMutationObserver();
    run();
    this.startMutationObserver();
  }

  /**
   * Schedule a node to be updated with a translation.
   *
   * @param {Node} node
   * @param {string} translatedHTML
   * @param {number} translationId - A unique id to identify this translation request.
   */
  scheduleNodeUpdateWithTranslation(node, translatedHTML, translationId) {
    // Add the nodes to be populated with the next translation update.
    this.#nodesWithTranslatedHTML.add({ node, translatedHTML, translationId });

    if (this.#pendingTranslationsCount === 0) {
      // No translations are pending, update the node.
      this.updateNodesWithTranslations();
    } else if (!this.#updateTimeout) {
      // Schedule an update.
      this.#updateTimeout = lazy.setTimeout(
        this.updateNodesWithTranslations.bind(this),
        DOM_UPDATE_INTERVAL_MS
      );
    } else {
      // An update has been previously scheduled, do nothing here.
    }
  }

  /**
   * Check to see if a language matches the document language.
   *
   * @param {Node} node
   */
  matchesDocumentLanguage(node) {
    if (!node.lang) {
      // No `lang` was present, so assume it matches the language.
      return true;
    }

    // First, cheaply check if language tags match, without canonicalizing.
    if (
      lazy.TranslationsUtils.langTagsMatch(this.documentLanguage, node.lang)
    ) {
      return true;
    }

    try {
      // Make sure the local is in the canonical form, and check again. This function
      // throws, so don't trust that the language tags are formatting correctly.
      const [language] = Intl.getCanonicalLocales(node.lang);

      return lazy.TranslationsUtils.langTagsMatch(
        this.documentLanguage,
        language
      );
    } catch (_error) {
      return false;
    }
  }
}

/**
 * This function needs to be fairly fast since it's used on many nodes when iterating
 * over the DOM to find nodes to translate.
 *
 * @param {Text | HTMLElement} node
 */
function isNodeHidden(node) {
  /** @type {HTMLElement} */
  const element = getElementForStyle(node);
  if (!element) {
    throw new Error("Unable to find the Element to compute the style for node");
  }

  // This flushes the style, which is a performance cost.
  const style = element.ownerGlobal.getComputedStyle(element);
  return style.display === "none" || style.visibility === "hidden";
}

/**
 * This function returns the correct element to determine the
 * style of node.
 *
 * @param {Node} node
  @returns {HTMLElement} */
function getElementForStyle(node) {
  if (node.nodeType != Node.TEXT_NODE) {
    return node;
  }

  if (node.parentElement) {
    return node.parentElement;
  }

  // For cases like text node where its parent is ShadowRoot,
  // we'd like to use flattenedTreeParentNode
  if (node.flattenedTreeParentNode) {
    return node.flattenedTreeParentNode;
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
 */
function isNodeInViewport(node) {
  const window = node.ownerGlobal;
  const document = node.ownerDocument;

  /** @type {HTMLElement} */
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
   * @type {Set<number>}
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
    element.setAttribute("value", element.value);
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

  merge(element, translationsDocument.body.firstChild);

  // Restore the <select> values.
  if (element.tagName === "SELECT") {
    element.value = nodeValues.get(element);
  }
  for (const select of element.querySelectorAll("select")) {
    select.value = nodeValues.get(select);
  }

  /**
   * Merge the live tree with the translated tree by re-using elements from the live tree.
   *
   * @param {Node} liveTree
   * @param {Node} translatedTree
   */
  function merge(liveTree, translatedTree) {
    /** @type {Map<number, Element>} */
    const liveElementsById = new Map();

    /** @type {Array<Text>} */
    const liveTextNodes = [];

    // Remove all the nodes from the liveTree, and categorize them by Text node or
    // Element node.
    let node;
    while ((node = liveTree.firstChild)) {
      node.remove();

      if (node.nodeType === Node.ELEMENT_NODE) {
        liveElementsById.set(node.dataset.mozTranslationsId, node);
      } else if (node.nodeType === Node.TEXT_NODE) {
        liveTextNodes.push(node);
      }
    }

    // The translated tree dictates the order.
    const translatedNodes = [...translatedTree.childNodes];
    for (
      let translatedIndex = 0;
      translatedIndex < translatedNodes.length;
      translatedIndex++
    ) {
      const translatedNode = translatedNodes[translatedIndex];

      if (translatedNode.nodeType === Node.TEXT_NODE) {
        // Copy the translated text to the original Text node and re-append it.
        let liveTextNode = liveTextNodes.shift();

        if (liveTextNode) {
          liveTextNode.data = translatedNode.data;
        } else {
          liveTextNode = translatedNode;
        }

        liveTree.appendChild(liveTextNode);
      } else if (translatedNode.nodeType === Node.ELEMENT_NODE) {
        const liveElementId = translatedNode.dataset.mozTranslationsId;
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
          liveElement = liveElement.cloneNode(true /* deep clone */);
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
            if (
              // Only consider other element nodes.
              sibling.nodeType === Node.ELEMENT_NODE &&
              // If the sibling's mozTranslationsId matches, then use the sibling's
              // node instead.
              liveElementId === sibling.dataset.mozTranslationsId
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
      // Clean-up the live element ids.
      delete node.dataset.mozTranslationsId;
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
          translatedHTML: translationsDocument.body.innerHTML,
          liveTree: liveTree.outerHTML,
          translatedTree: translatedTree.outerHTML,
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
 * @param {Node | null} root
 */
function createNodePath(node, root) {
  if (root === null) {
    root = node.ownerDocument.body;
  }
  let path =
    node.parentNode && node.parentNode !== root
      ? createNodePath(node.parentNode)
      : "";
  path += `/${node.nodeName}`;
  if (node.id) {
    path += `#${node.id}`;
  } else if (node.className) {
    for (const className of node.classList) {
      path += "." + className;
    }
  }
  return path;
}

/**
 * @param {Node} node
 * @returns {boolean}
 */
function isNodeTextEmpty(node) {
  if ("innerText" in node) {
    return node.innerText.trim().length === 0;
  }
  if (node.nodeType === Node.TEXT_NODE && node.nodeValue) {
    return node.nodeValue.trim().length === 0;
  }
  return true;
}

/**
 * @param {Node} node
 */
function removeTextNodes(node) {
  for (const child of node.childNodes) {
    switch (child.nodeType) {
      case Node.TEXT_NODE:
        node.removeChild(child);
        break;
      case Node.ELEMENT_NODE:
        removeTextNodes(child);
        break;
      default:
        break;
    }
  }
}

/**
 * Test whether any of the direct child text nodes of are non-whitespace
 * text nodes.
 *
 * For example:
 *   - `<p>test</p>`: yes
 *   - `<p> </p>`: no
 *   - `<p><b>test</b></p>`: no
 *
 * @param {Node} node
 * @returns {boolean}
 */
function hasTextNodes(node) {
  if (node.nodeType !== Node.ELEMENT_NODE) {
    // Only check element nodes.
    return false;
  }

  for (const child of node.childNodes) {
    if (child.nodeType === Node.TEXT_NODE) {
      if (child.textContent.trim() === "") {
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
 * Like `isExcludedNode` but looks at the full subtree. Used to see whether
 * we can submit a subtree, or whether we should split it into smaller
 * branches first to try to exclude more of the non-translatable content.
 *
 * @param {Node} node
 * @param {string} excludedNodeSelector
 * @returns {boolean}
 */
function containsExcludedNode(node, excludedNodeSelector) {
  return (
    node.nodeType === Node.ELEMENT_NODE &&
    node.querySelector(excludedNodeSelector)
  );
}

/**
 * Check if this node has already been queued to be translated. This can be because
 * the node is itself is queued, or its parent node is queued.
 *
 * @param {Node} node
 * @param {Map<Node, any>} queuedNodes
 * @returns {boolean}
 */
function isNodeQueued(node, queuedNodes) {
  if (queuedNodes.has(node)) {
    return true;
  }

  // If the immediate parent is the body, it is allowed.
  if (node.parentNode === node.ownerDocument.body) {
    return false;
  }

  // Accessing the parentNode is expensive here according to performance profilling. This
  // is due to XrayWrappers. Minimize reading attributes by storing a reference to the
  // `parentNode` in a named variable, rather than re-accessing it.
  let parentNode;
  let lastNode = node;
  while ((parentNode = lastNode.parentNode)) {
    if (queuedNodes.has(parentNode)) {
      return parentNode;
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
 * @param {Element} element
 */
function getIsBlockLike(element) {
  const win = element.ownerGlobal;
  if (element.namespaceURI === "http://www.w3.org/2000/svg") {
    // SVG elements will report as inline, but there is no block layout in SVG.
    // Treat every SVG element as being block so that every node will be subdivided.
    return true;
  }
  const { display } = win.getComputedStyle(element);
  return display !== "inline" && display !== "none";
}

/**
 * Determine if this element is an inline element or a block element. Inline elements
 * should be sent as a contiguous chunk of text, while block elements should be further
 * subdivided before sending them in for translation.
 *
 * @param {Node} node
 * @returns {boolean}
 */
function nodeNeedsSubdividing(node) {
  if (node.nodeType === Node.TEXT_NODE) {
    // Text nodes are fully subdivided.
    return false;
  }

  if (!getIsBlockLike(node)) {
    // This element is inline, or not displayed.
    return false;
  }

  for (let child of node.childNodes) {
    switch (child.nodeType) {
      case Node.TEXT_NODE:
        // Keep checking for more inline or text nodes.
        continue;
      case Node.ELEMENT_NODE: {
        if (getIsBlockLike(child)) {
          // This node is a block node, so it needs further subdividing.
          return true;
        }
        // Keep checking for more inline or text nodes.
        continue;
      }
      default:
        return true;
    }
  }
  return false;
}

/**
 * Returns an iterator of a node's ancestors.
 *
 * @param {Node} node
 * @returns {Generator<ParentNode>}
 */
function* getAncestorsIterator(node) {
  const document = node.ownerDocument;
  for (
    let parent = node.parentNode;
    parent && parent !== document.documentElement;
    parent = parent.parentNode
  ) {
    yield parent;
  }
}

/**
 * This contains all of the information needed to perform a translation request.
 *
 * @typedef {object} TranslationRequest
 * @property {Node} node
 * @property {string} sourceText
 * @property {number} translationId
 * @property {boolean} isHTML
 * @property {Function} resolve
 * @property {Function} reject
 */

/**
 * When a page is hidden, mutations may occur in the DOM. It doesn't make sense to
 * translate those elements while the page is hidden, especially as it may bring
 * a translations engine back to life, which can be quite expensive. Queue those
 * messages here.
 */
class QueuedTranslator {
  /**
   * @type {MessagePort | null}
   */
  #port = null;

  /**
   * @type {() => void}
   */
  #actorRequestNewPort;

  /**
   * Send a message to the actor that the first visible DOM translation change is about to occur.
   *
   * @type {() => void}
   */
  #actorReportFirstVisibleChange;

  /**
   * Tie together a message id to a resolved response.
   *
   * @type {Map<number, TranslationRequest>}
   */
  #requests = new Map();

  /**
   * If the translations are paused, they are queued here. This Map is ordered by
   * from oldest to newest requests with stale requests being removed.
   *
   * @type {Map<Node, TranslationRequest>}
   */
  #queue = new Map();

  /**
   * @type {"uninitialized" | "ready" | "error" | "closed"}
   */
  engineStatus = "uninitialized";

  /**
   * @param {MessagePort} port
   * @param {() => void} actorRequestNewPort
   * @param {() => void} actorReportFirstVisibleChange
   */
  constructor(port, actorRequestNewPort, actorReportFirstVisibleChange) {
    this.#actorRequestNewPort = actorRequestNewPort;
    this.#actorReportFirstVisibleChange = actorReportFirstVisibleChange;

    this.acquirePort(port);
  }

  /**
   * When an engine gets closed while still in use, a new one will need to be requested.
   *
   * @type {{ promise: Promise<void>, resolve: Function, reject: Function } | null}
   */
  #portRequest = null;

  /**
   * Keep track if the page is shown or hidden. When the page is hidden, no translations
   * will be posted to the translations engine.
   */
  #isPageShown = true;

  /**
   * Note when a new port is being requested so we don't re-request it.
   */
  async showPage() {
    this.#isPageShown = true;
    if (this.#port) {
      throw new Error(
        "Attempting to show the page when there is already port available"
      );
    }

    let portRequestPromise;
    if (this.#portRequest) {
      // It is possible that the page is being re-shown while a port request is still pending.
      // If that is the case, then we should continue to wait for the pending port.
      portRequestPromise = this.#portRequest.promise;
    } else if (this.#queue.size) {
      // There are queued translations, request a new port. After the port is retrieved
      // the pending queue will be processed.
      portRequestPromise = this.#requestNewPort();
    }

    try {
      await portRequestPromise;
    } catch {
      // Failed to retrieve the port after re-showing a page, which will be reported as an error in the panel UI.
      // At this point it is up to the user to determine the next step from the UI.
    }
  }

  /**
   * Hide the page, and move any outstanding translation requests to a queue.
   */
  async hidePage() {
    this.#isPageShown = false;

    if (this.#portRequest) {
      // It is possible that the page is being hidden while a port request is still pending.
      // If that is the case, then we should wait for the port to resolve so that any pending
      // translations can be properly moved to the queue, ready to resume when the page is re-shown.
      try {
        await this.#portRequest.promise;
      } catch {
        // Failed to retrieve the port after hiding the page. At this point it is up to the user to
        // determine the next step from the UI if they return to the page that was hidden.
      }
    }

    if (this.#requests.size) {
      lazy.console.log(
        "Pausing translations with pending translation requests."
      );
      this.#moveRequestsToQueue();
    }

    this.discardPort();
  }

  /**
   * Request a new port. The port will come in via `acquirePort`, and then resolved
   * through the `this.#portRequest.resolve`.
   *
   * @returns {Promise<void>}
   */
  #requestNewPort() {
    if (this.#portRequest) {
      // A port was already requested.
      return this.#portRequest.promise;
    }

    const portRequest = { promise: null, resolve: null, reject: null };
    portRequest.promise = new Promise((resolve, reject) => {
      portRequest.resolve = resolve;
      portRequest.reject = reject;
    });

    this.#portRequest = portRequest;

    // Send a request through the actor for a new port. The request response will
    // trigger the method `QueuedTranslator.prototype.acquirePort`
    this.#actorRequestNewPort();

    this.#portRequest.promise
      .then(
        () => {
          if (portRequest === this.#portRequest) {
            this.#portRequest = null;
          }

          // Resume the queued translations.
          if (this.#queue.size) {
            lazy.console.log(
              `Resuming ${
                this.#queue.size
              } translations from the pending translation queue.`
            );

            const oldQueue = this.#queue;
            this.#queue = new Map();
            this.#repostTranslations(oldQueue.values());
          }
        },
        error => {
          lazy.console.error(error);
        }
      )
      .finally(() => {
        if (portRequest === this.#portRequest) {
          this.#portRequest = null;
        }
      });

    return portRequest.promise;
  }

  /**
   * Send a request to translate text to the Translations Engine. If it returns `null`
   * then the request is stale. A rejection means there was an error in the translation.
   * This request may be queued.
   *
   * @param {Node} node
   * @param {string} sourceText
   * @param {boolean} isHTML
   * @param {number} translationId
   * @returns {{ messageId: number, translation: Promise<string>}}
   */
  async translate(node, sourceText, isHTML, translationId) {
    if (this.#isPageShown && !this.#port) {
      try {
        await this.#requestNewPort();
      } catch {}
    }

    // At this point we don't know if the page is still shown, or if the attempt
    // to get a port was successful so check again.

    if (!this.#isPageShown || !this.#port) {
      // Queue the request while the page isn't shown.
      return new Promise((resolve, reject) => {
        const previousRequest = this.#queue.get(node);
        if (previousRequest) {
          // Previous requests get resolved as null, as this new one will replace it.
          previousRequest.resolve(null);
          // Delete the entry so that the order of the queue is maintained. The
          // new request will be put on the end.
          this.#queue.delete(node);
        }

        // This Promises's resolve and reject will be chained after the translation
        // request. For now add it to the queue along with the other arguments.
        this.#queue.set(node, {
          node,
          sourceText,
          isHTML,
          translationId,
          resolve,
          reject,
        });
      });
    }

    return this.#postTranslationRequest(
      node,
      sourceText,
      isHTML,
      translationId
    );
  }

  /**
   * @param {number} translationId
   */
  async cancelSingleTranslation(translationId) {
    this.#port.postMessage({
      type: "TranslationsPort:CancelSingleTranslation",
      translationId,
    });
  }

  /**
   * Posts the translation to the translations engine through the MessagePort.
   *
   * @param {Node} node
   * @param {string} sourceText
   * @param {boolean} isHTML
   * @param {number} translationId
   * @returns {{ translateText: TranslationFunction, translateHTML: TranslationFunction}}
   */
  #postTranslationRequest(node, sourceText, isHTML, translationId) {
    return new Promise((resolve, reject) => {
      // Store the "resolve" for the promise. It will be matched back up with the
      // `translationId` in #handlePortMessage.
      this.#requests.set(translationId, {
        node,
        sourceText,
        isHTML,
        translationId,
        resolve,
        reject,
      });
      this.#port.postMessage({
        type: "TranslationsPort:TranslationRequest",
        translationId,
        sourceText,
        isHTML,
      });
    });
  }

  /**
   * Close the port and move any pending translations onto a queue.
   */
  discardPort() {
    if (this.#port) {
      this.#port.postMessage({ type: "TranslationsPort:DiscardTranslations" });
      this.#port.close();
      this.#port = null;
      this.#portRequest = null;
    }
    this.#moveRequestsToQueue();
    this.engineStatus = "uninitialized";
  }

  /**
   * Move any unfulfilled requests to the queue so they can be sent again when
   * the page is active again.
   */
  #moveRequestsToQueue() {
    if (this.#requests.size) {
      for (const request of this.#requests.values()) {
        this.#queue.set(request.node, request);
      }
      this.#requests = new Map();
    }
  }

  /**
   * Acquires a port, checks on the engine status, and then starts or resumes
   * translations.
   *
   * @param {MessagePort} port
   */
  acquirePort(port) {
    if (this.#port) {
      if (this.engineStatus === "ready") {
        lazy.console.error(
          "Received a new translation port while one already existed."
        );
      }
      this.discardPort();
    }

    this.#port = port;

    const portRequest = this.#portRequest;

    // Match up a response on the port to message that was sent.
    port.onmessage = ({ data }) => {
      switch (data.type) {
        case "TranslationsPort:TranslationResponse": {
          if (!this.hasFirstVisibleChange) {
            this.hasFirstVisibleChange = true;
            this.#actorReportFirstVisibleChange();
          }
          const { targetText, translationId } = data;
          // A request may not match match a translationId if there is a race during
          // the pausing and discarding of the queue.
          this.#requests.get(translationId)?.resolve(targetText);
          this.#requests.delete(translationId);
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
          this.engineStatus = data.status;
          break;
        }
        case "TranslationsPort:EngineTerminated": {
          // The engine was terminated, and if a translation is needed a new port
          // will need to be requested.
          this.engineStatus = "closed";
          this.discardPort();
          if (this.#queue.size && this.#isPageShown) {
            this.#requestNewPort();
          }
          break;
        }
        default:
          lazy.console.error("Unknown translations port message: " + data.type);
          break;
      }
    };

    port.postMessage({ type: "TranslationsPort:GetEngineStatusRequest" });
  }

  /**
   * Re-send a list of translation requests.
   *
   * @param {Iterator<TranslationRequest>} translationRequests
   *  This is either the this.#queue or this.#requests.
   */
  #repostTranslations(translationRequests) {
    for (const request of translationRequests) {
      const { node, sourceText, isHTML, translationId, resolve, reject } =
        request;
      if (isNodeDetached(node)) {
        // If the node is dead, resolve without any text. Do not reject as that
        // will be treated as an error.
        resolve(null);
      } else {
        this.#postTranslationRequest(
          node,
          sourceText,
          isHTML,
          translationId
        ).then(resolve, reject);
      }
    }
  }

  /**
   * Close the port and remove any pending or queued requests.
   */
  destroy() {
    this.#port.close();
    this.#requests = new Map();
    this.#queue = new Map();
  }
}

/**
 * Determines whether an attribute on a given element is translatable based on the specified
 * criteria for TRANSLATABLE_ATTRIBUTES.
 *
 * @see TRANSLATABLE_ATTRIBUTES
 *
 * @param {Element} element - The DOM element on which the attribute is being checked.
 * @param {string} attribute - The attribute name to check for translatability.
 *
 * @returns {boolean}
 */
function isAttributeTranslatable(element, attribute) {
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
 * @param {Node} node
 */
function isNodeDetached(node) {
  return (
    // This node is out of the DOM and already garbage collected.
    Cu.isDeadWrapper(node) ||
    // Normally you could just check `node.parentElement` to see if an element is
    // part of the DOM, but the Chrome-only flattenedTreeParentNode is used to include
    // Shadow DOM elements, which have a null parentElement.
    !node.flattenedTreeParentNode
  );
}
