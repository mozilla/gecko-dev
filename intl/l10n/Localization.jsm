/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */

/* Copyright 2017 Mozilla Foundation and others
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/* fluent-dom@fa25466f (October 12, 2018) */

/* eslint no-console: ["error", { allow: ["warn", "error"] }] */
/* global console */

const { L10nRegistry } = ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm", {});
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});
const { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm", {});

/*
 * Base CachedIterable class.
 */
class CachedIterable extends Array {
  /**
   * Create a `CachedIterable` instance from an iterable or, if another
   * instance of `CachedIterable` is passed, return it without any
   * modifications.
   *
   * @param {Iterable} iterable
   * @returns {CachedIterable}
   */
  static from(iterable) {
    if (iterable instanceof this) {
      return iterable;
    }

    return new this(iterable);
  }
}

/*
 * CachedAsyncIterable caches the elements yielded by an async iterable.
 *
 * It can be used to iterate over an iterable many times without depleting the
 * iterable.
 */
class CachedAsyncIterable extends CachedIterable {
  /**
   * Create an `CachedAsyncIterable` instance.
   *
   * @param {Iterable} iterable
   * @returns {CachedAsyncIterable}
   */
  constructor(iterable) {
    super();

    if (Symbol.asyncIterator in Object(iterable)) {
      this.iterator = iterable[Symbol.asyncIterator]();
    } else if (Symbol.iterator in Object(iterable)) {
      this.iterator = iterable[Symbol.iterator]();
    } else {
      throw new TypeError("Argument must implement the iteration protocol.");
    }
  }

  /**
   * Asynchronous iterator caching the yielded elements.
   *
   * Elements yielded by the original iterable will be cached and available
   * synchronously. Returns an async generator object implementing the
   * iterator protocol over the elements of the original (async or sync)
   * iterable.
   */
  [Symbol.asyncIterator]() {
    const cached = this;
    let cur = 0;

    return {
      async next() {
        if (cached.length <= cur) {
          cached.push(cached.iterator.next());
        }
        return cached[cur++];
      },
    };
  }

  /**
   * This method allows user to consume the next element from the iterator
   * into the cache.
   *
   * @param {number} count - number of elements to consume
   */
  async touchNext(count = 1) {
    let idx = 0;
    while (idx++ < count) {
      const last = this[this.length - 1];
      if (last && (await last).done) {
        break;
      }
      this.push(this.iterator.next());
    }
    // Return the last cached {value, done} object to allow the calling
    // code to decide if it needs to call touchNext again.
    return this[this.length - 1];
  }
}

/**
 * The default localization strategy for Gecko. It comabines locales
 * available in L10nRegistry, with locales requested by the user to
 * generate the iterator over FluentBundles.
 *
 * In the future, we may want to allow certain modules to override this
 * with a different negotitation strategy to allow for the module to
 * be localized into a different language - for example DevTools.
 */
function defaultGenerateBundles(resourceIds) {
  const appLocales = Services.locale.appLocalesAsBCP47;
  return L10nRegistry.generateBundles(appLocales, resourceIds);
}

/**
 * The `Localization` class is a central high-level API for vanilla
 * JavaScript use of Fluent.
 * It combines language negotiation, FluentBundle and I/O to
 * provide a scriptable API to format translations.
 */
class Localization {
  /**
   * @param {Array<String>} resourceIds     - List of resource IDs
   * @param {Function}      generateBundles - Function that returns a
   *                                          generator over FluentBundles
   *
   * @returns {Localization}
   */
  constructor(resourceIds = [], generateBundles = defaultGenerateBundles) {
    this.resourceIds = resourceIds;
    this.generateBundles = generateBundles;
    this.bundles = CachedAsyncIterable.from(
      this.generateBundles(this.resourceIds));
  }

  /**
   * @param {Array<String>} resourceIds - List of resource IDs
   * @param {bool}                eager - whether the I/O for new context should
   *                                      begin eagerly
   */
  addResourceIds(resourceIds, eager = false) {
    this.resourceIds.push(...resourceIds);
    this.onChange(eager);
    return this.resourceIds.length;
  }

  removeResourceIds(resourceIds) {
    this.resourceIds = this.resourceIds.filter(r => !resourceIds.includes(r));
    this.onChange();
    return this.resourceIds.length;
  }

  /**
   * Format translations and handle fallback if needed.
   *
   * Format translations for `keys` from `FluentBundle` instances on this
   * DOMLocalization. In case of errors, fetch the next context in the
   * fallback chain.
   *
   * @param   {Array<Object>}         keys    - Translation keys to format.
   * @param   {Function}              method  - Formatting function.
   * @returns {Promise<Array<string|Object>>}
   * @private
   */
  async formatWithFallback(keys, method) {
    const translations = [];

    for await (const bundle of this.bundles) {
      const missingIds = keysFromBundle(method, bundle, keys, translations);

      if (missingIds.size === 0) {
        break;
      }

      if (AppConstants.NIGHTLY_BUILD || Cu.isInAutomation) {
        const locale = bundle.locales[0];
        const ids = Array.from(missingIds).join(", ");
        if (Cu.isInAutomation) {
          throw new Error(`Missing translations in ${locale}: ${ids}`);
        }
        console.warn(`Missing translations in ${locale}: ${ids}`);
      }
    }

    return translations;
  }

  /**
   * Format translations into {value, attributes} objects.
   *
   * The fallback logic is the same as in `formatValues` but the argument type
   * is stricter (an array of arrays) and it returns {value, attributes}
   * objects which are suitable for the translation of DOM elements.
   *
   *     docL10n.formatMessages([
   *       {id: 'hello', args: { who: 'Mary' }},
   *       {id: 'welcome'}
   *     ]).then(console.log);
   *
   *     // [
   *     //   { value: 'Hello, Mary!', attributes: null },
   *     //   {
   *     //     value: 'Welcome!',
   *     //     attributes: [ { name: "title", value: 'Hello' } ]
   *     //   }
   *     // ]
   *
   * Returns a Promise resolving to an array of the translation strings.
   *
   * @param   {Array<Object>} keys
   * @returns {Promise<Array<{value: string, attributes: Object}>>}
   * @private
   */
  formatMessages(keys) {
    return this.formatWithFallback(keys, messageFromBundle);
  }

  /**
   * Retrieve translations corresponding to the passed keys.
   *
   * A generalized version of `DOMLocalization.formatValue`. Keys can
   * either be simple string identifiers or `[id, args]` arrays.
   *
   *     docL10n.formatValues([
   *       {id: 'hello', args: { who: 'Mary' }},
   *       {id: 'hello', args: { who: 'John' }},
   *       {id: 'welcome'}
   *     ]).then(console.log);
   *
   *     // ['Hello, Mary!', 'Hello, John!', 'Welcome!']
   *
   * Returns a Promise resolving to an array of the translation strings.
   *
   * @param   {Array<Object>} keys
   * @returns {Promise<Array<string>>}
   */
  formatValues(keys) {
    return this.formatWithFallback(keys, valueFromBundle);
  }

  /**
   * Retrieve the translation corresponding to the `id` identifier.
   *
   * If passed, `args` is a simple hash object with a list of variables that
   * will be interpolated in the value of the translation.
   *
   *     docL10n.formatValue(
   *       'hello', { who: 'world' }
   *     ).then(console.log);
   *
   *     // 'Hello, world!'
   *
   * Returns a Promise resolving to the translation string.
   *
   * Use this sparingly for one-off messages which don't need to be
   * retranslated when the user changes their language preferences, e.g. in
   * notifications.
   *
   * @param   {string}  id     - Identifier of the translation to format
   * @param   {Object}  [args] - Optional external arguments
   * @returns {Promise<string>}
   */
  async formatValue(id, args) {
    const [val] = await this.formatValues([{id, args}]);
    return val;
  }

  /**
   * Register weak observers on events that will trigger cache invalidation
   */
  registerObservers() {
    Services.obs.addObserver(this, "intl:app-locales-changed", true);
    Services.prefs.addObserver("intl.l10n.pseudo", this, true);
  }

  /**
   * Default observer handler method.
   *
   * @param {String} subject
   * @param {String} topic
   * @param {Object} data
   */
  observe(subject, topic, data) {
    switch (topic) {
      case "intl:app-locales-changed":
        this.onChange();
        break;
      case "nsPref:changed":
        switch (data) {
          case "intl.l10n.pseudo":
            this.onChange();
        }
        break;
      default:
        break;
    }
  }

  /**
   * This method should be called when there's a reason to believe
   * that language negotiation or available resources changed.
   *
   * @param {bool} eager - whether the I/O for new context should begin eagerly
   */
  onChange(eager = false) {
    this.bundles = CachedAsyncIterable.from(
      this.generateBundles(this.resourceIds));
    if (eager) {
      // If the first app locale is the same as last fallback
      // it means that we have all resources in this locale, and
      // we want to eagerly fetch just that one.
      // Otherwise, we're in a scenario where the first locale may
      // be partial and we want to eagerly fetch a fallback as well.
      const appLocale = Services.locale.appLocaleAsBCP47;
      const lastFallback = Services.locale.lastFallbackLocale;
      const prefetchCount = appLocale === lastFallback ? 1 : 2;
      this.bundles.touchNext(prefetchCount);
    }
  }
}

Localization.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsISupportsWeakReference,
]);

/**
 * Format the value of a message into a string.
 *
 * This function is passed as a method to `keysFromBundle` and resolve
 * a value of a single L10n Entity using provided `FluentBundle`.
 *
 * If the function fails to retrieve the entity, it will return an ID of it.
 * If formatting fails, it will return a partially resolved entity.
 *
 * In both cases, an error is being added to the errors array.
 *
 * @param   {FluentBundle} bundle
 * @param   {Array<Error>}   errors
 * @param   {string}         id
 * @param   {Object}         args
 * @returns {string}
 * @private
 */
function valueFromBundle(bundle, errors, id, args) {
  const msg = bundle.getMessage(id);
  return bundle.format(msg, args, errors);
}

/**
 * Format all public values of a message into a {value, attributes} object.
 *
 * This function is passed as a method to `keysFromBundle` and resolve
 * a single L10n Entity using provided `FluentBundle`.
 *
 * The function will return an object with a value and attributes of the
 * entity.
 *
 * If the function fails to retrieve the entity, the value is set to the ID of
 * an entity, and attributes to `null`. If formatting fails, it will return
 * a partially resolved value and attributes.
 *
 * In both cases, an error is being added to the errors array.
 *
 * @param   {FluentBundle} bundle
 * @param   {Array<Error>}   errors
 * @param   {String}         id
 * @param   {Object}         args
 * @returns {Object}
 * @private
 */
function messageFromBundle(bundle, errors, id, args) {
  const msg = bundle.getMessage(id);

  const formatted = {
    value: bundle.format(msg, args, errors),
    attributes: null,
  };

  if (msg.attrs) {
    formatted.attributes = [];
    for (const [name, attr] of Object.entries(msg.attrs)) {
      const value = bundle.format(attr, args, errors);
      if (value !== null) {
        formatted.attributes.push({name, value});
      }
    }
  }

  return formatted;
}

/**
 * This function is an inner function for `Localization.formatWithFallback`.
 *
 * It takes a `FluentBundle`, list of l10n-ids and a method to be used for
 * key resolution (either `valueFromBundle` or `messageFromBundle`) and
 * optionally a value returned from `keysFromBundle` executed against
 * another `FluentBundle`.
 *
 * The idea here is that if the previous `FluentBundle` did not resolve
 * all keys, we're calling this function with the next context to resolve
 * the remaining ones.
 *
 * In the function, we loop over `keys` and check if we have the `prev`
 * passed and if it has an error entry for the position we're in.
 *
 * If it doesn't, it means that we have a good translation for this key and
 * we return it. If it does, we'll try to resolve the key using the passed
 * `FluentBundle`.
 *
 * In the end, we fill the translations array, and return the Set with
 * missing ids.
 *
 * See `Localization.formatWithFallback` for more info on how this is used.
 *
 * @param {Function}       method
 * @param {FluentBundle}   bundle
 * @param {Array<string>}  keys
 * @param {{Array<{value: string, attributes: Object}>}} translations
 *
 * @returns {Set<string>}
 * @private
 */
function keysFromBundle(method, bundle, keys, translations) {
  const messageErrors = [];
  const missingIds = new Set();

  keys.forEach(({id, args}, i) => {
    if (translations[i] !== undefined) {
      return;
    }

    if (bundle.hasMessage(id)) {
      messageErrors.length = 0;
      translations[i] = method(bundle, messageErrors, id, args);
      // XXX: Report resolver errors
    } else {
      missingIds.add(id);
    }
  });

  return missingIds;
}

this.Localization = Localization;
var EXPORTED_SYMBOLS = ["Localization"];
