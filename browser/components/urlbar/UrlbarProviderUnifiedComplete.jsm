/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that wraps the existing UnifiedComplete
 * component, it is supposed to be used as an interim solution while we rewrite
 * the model providers in a more modular way.
 */

var EXPORTED_SYMBOLS = ["UrlbarProviderUnifiedComplete"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "unifiedComplete",
  "@mozilla.org/autocomplete/search;1?name=unifiedcomplete",
  "nsIAutoCompleteSearch"
);

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Urlbar.Provider.UnifiedComplete")
);

/**
 * Class used to create the provider.
 */
class ProviderUnifiedComplete extends UrlbarProvider {
  constructor() {
    super();
    // Maps the running queries by queryContext.
    this.queries = new Map();
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "UnifiedComplete";
  }

  /**
   * Returns the type of this provider.
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.IMMEDIATE;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    return true;
  }

  /**
   * Whether this provider wants to restrict results to just itself.
   * Other providers won't be invoked, unless this provider doesn't
   * support the current query.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider wants to restrict results.
   */
  isRestricting(queryContext) {
    return false;
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        match.
   * @returns {Promise} resolved when the query stops.
   */
  async startQuery(queryContext, addCallback) {
    logger.info(`Starting query for ${queryContext.searchString}`);
    let instance = {};
    this.queries.set(queryContext, instance);

    // Supported search params are:
    //  * "enable-actions": default to true.
    //  * "disable-private-actions": set for private windows, if not in permanent
    //    private browsing mode. ()
    //  * "private-window": the search is taking place in a private window.
    //  * "prohibit-autofill": disable autofill, i.e., the first (heuristic)
    //    result should never be an autofill result.
    //  * "user-context-id:#": the userContextId to use.
    let params = ["enable-actions"];
    params.push(`max-results:${queryContext.maxResults}`);
    // This is necessary because we insert matches one by one, thus we don't
    // want UnifiedComplete to reuse results.
    params.push(`insert-method:${UrlbarUtils.INSERTMETHOD.APPEND}`);
    // The Quantum Bar has its own telemetry measurement, thus disable old
    // telemetry logged by UnifiedComplete.
    params.push("disable-telemetry");
    if (queryContext.isPrivate) {
      params.push("private-window");
      if (!PrivateBrowsingUtils.permanentPrivateBrowsing) {
        params.push("disable-private-actions");
      }
    }
    if (queryContext.userContextId) {
      params.push(`user-context-id:${queryContext.userContextId}}`);
    }
    if (!queryContext.allowAutofill) {
      params.push("prohibit-autofill");
    }

    let urls = new Set();
    await new Promise(resolve => {
      let listener = {
        onSearchResult(_, result) {
          let { done, matches } = convertResultToMatches(
            queryContext,
            result,
            urls
          );
          for (let match of matches) {
            addCallback(UrlbarProviderUnifiedComplete, match);
          }
          if (done) {
            delete this._resolveSearch;
            resolve();
          }
        },
      };
      this._resolveSearch = resolve;
      unifiedComplete.startSearch(
        queryContext.searchString,
        params.join(" "),
        null, // previousResult
        listener
      );
    });

    // We are done.
    this.queries.delete(queryContext);
  }

  /**
   * Cancels a running query.
   * @param {object} queryContext The query context object
   */
  cancelQuery(queryContext) {
    logger.info(`Canceling query for ${queryContext.searchString}`);
    // This doesn't properly support being used concurrently by multiple fields.
    this.queries.delete(queryContext);
    unifiedComplete.stopSearch();
    if (this._resolveSearch) {
      this._resolveSearch();
    }
  }
}

var UrlbarProviderUnifiedComplete = new ProviderUnifiedComplete();

/**
 * Convert from a nsIAutocompleteResult to a list of new matches.
 * Note that at every call we get the full set of matches, included the
 * previously returned ones, and new matches may be inserted in the middle.
 * This means we could sort these wrongly, the muxer should take care of it.
 * In any case at least we're sure there's just one heuristic result and it
 * comes first.
 *
 * @param {UrlbarQueryContext} context the query context.
 * @param {object} result an nsIAutocompleteResult
 * @param {set} urls a Set containing all the found urls, used to discard
 *        already added matches.
 * @returns {object} { matches: {array}, done: {boolean} }
 */
function convertResultToMatches(context, result, urls) {
  let matches = [];
  let done =
    [
      Ci.nsIAutoCompleteResult.RESULT_IGNORED,
      Ci.nsIAutoCompleteResult.RESULT_FAILURE,
      Ci.nsIAutoCompleteResult.RESULT_NOMATCH,
      Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
    ].includes(result.searchResult) || result.errorDescription;

  for (let i = 0; i < result.matchCount; ++i) {
    // First, let's check if we already added this match.
    // nsIAutocompleteResult always contains all of the matches, includes ones
    // we may have added already. This means we'll end up adding things in the
    // wrong order here, but that's a task for the UrlbarMuxer.
    let url = result.getFinalCompleteValueAt(i);
    if (urls.has(url)) {
      continue;
    }
    urls.add(url);
    let style = result.getStyleAt(i);
    let isHeuristic = i == 0 && style.includes("heuristic");
    let match = makeUrlbarResult(context.tokens, {
      url,
      icon: result.getImageAt(i),
      style,
      comment: result.getCommentAt(i),
      firstToken: context.tokens[0],
      isHeuristic,
    });
    // Should not happen, but better safe than sorry.
    if (!match) {
      continue;
    }
    // Manage autofill and preselected properties for the first match.
    if (isHeuristic) {
      if (style.includes("autofill") && result.defaultIndex == 0) {
        let autofillValue = result.getValueAt(i);
        if (
          autofillValue
            .toLocaleLowerCase()
            .startsWith(context.searchString.toLocaleLowerCase())
        ) {
          match.autofill = {
            value:
              context.searchString +
              autofillValue.substring(context.searchString.length),
            selectionStart: context.searchString.length,
            selectionEnd: autofillValue.length,
          };
        }
      }

      context.preselected = true;
      match.heuristic = true;
    }
    matches.push(match);
  }
  return { matches, done };
}

/**
 * Creates a new UrlbarResult from the provided data.
 * @param {array} tokens the search tokens.
 * @param {object} info includes properties from the legacy match.
 * @returns {object} an UrlbarResult
 */
function makeUrlbarResult(tokens, info) {
  let action = PlacesUtils.parseActionUrl(info.url);
  if (action) {
    switch (action.type) {
      case "searchengine": {
        let keywordOffer = UrlbarUtils.KEYWORD_OFFER.NONE;
        if (
          action.params.alias &&
          !action.params.searchQuery.trim() &&
          action.params.alias.startsWith("@")
        ) {
          keywordOffer = info.isHeuristic
            ? UrlbarUtils.KEYWORD_OFFER.HIDE
            : UrlbarUtils.KEYWORD_OFFER.SHOW;
        }
        return new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.SEARCH,
          UrlbarUtils.RESULT_SOURCE.SEARCH,
          ...UrlbarResult.payloadAndSimpleHighlights(tokens, {
            engine: [action.params.engineName, UrlbarUtils.HIGHLIGHT.TYPED],
            suggestion: [
              action.params.searchSuggestion,
              UrlbarUtils.HIGHLIGHT.SUGGESTED,
            ],
            keyword: [action.params.alias, UrlbarUtils.HIGHLIGHT.TYPED],
            query: [
              action.params.searchQuery.trim(),
              UrlbarUtils.HIGHLIGHT.TYPED,
            ],
            icon: [info.icon],
            keywordOffer,
          })
        );
      }
      case "keyword": {
        let title = info.comment;
        if (!title) {
          // If the url doesn't have an host (e.g. javascript urls), comment
          // will be empty, and we can't build the usual title. Thus use the url.
          title = Services.textToSubURI.unEscapeURIForUI(
            "UTF-8",
            action.params.url
          );
        } else if (tokens && tokens.length > 1) {
          title = UrlbarUtils.strings.formatStringFromName(
            "bookmarkKeywordSearch",
            [
              info.comment,
              tokens
                .slice(1)
                .map(t => t.value)
                .join(" "),
            ]
          );
        }
        return new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.KEYWORD,
          UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
          ...UrlbarResult.payloadAndSimpleHighlights(tokens, {
            title: [title, UrlbarUtils.HIGHLIGHT.TYPED],
            url: [action.params.url, UrlbarUtils.HIGHLIGHT.TYPED],
            keyword: [info.firstToken.value, UrlbarUtils.HIGHLIGHT.TYPED],
            input: [action.params.input],
            postData: [action.params.postData],
            icon: [info.icon],
          })
        );
      }
      case "extension":
        return new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.OMNIBOX,
          UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
          ...UrlbarResult.payloadAndSimpleHighlights(tokens, {
            title: [info.comment, UrlbarUtils.HIGHLIGHT.TYPED],
            content: [action.params.content, UrlbarUtils.HIGHLIGHT.TYPED],
            keyword: [action.params.keyword, UrlbarUtils.HIGHLIGHT.TYPED],
            icon: [info.icon],
          })
        );
      case "remotetab":
        return new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.REMOTE_TAB,
          UrlbarUtils.RESULT_SOURCE.TABS,
          ...UrlbarResult.payloadAndSimpleHighlights(tokens, {
            url: [action.params.url, UrlbarUtils.HIGHLIGHT.TYPED],
            title: [info.comment, UrlbarUtils.HIGHLIGHT.TYPED],
            device: [action.params.deviceName, UrlbarUtils.HIGHLIGHT.TYPED],
            icon: [info.icon],
          })
        );
      case "switchtab":
        return new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
          UrlbarUtils.RESULT_SOURCE.TABS,
          ...UrlbarResult.payloadAndSimpleHighlights(tokens, {
            url: [action.params.url, UrlbarUtils.HIGHLIGHT.TYPED],
            title: [info.comment, UrlbarUtils.HIGHLIGHT.TYPED],
            device: [action.params.deviceName, UrlbarUtils.HIGHLIGHT.TYPED],
            icon: [info.icon],
          })
        );
      case "visiturl":
        return new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.URL,
          UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          ...UrlbarResult.payloadAndSimpleHighlights(tokens, {
            title: [info.comment, UrlbarUtils.HIGHLIGHT.TYPED],
            url: [action.params.url, UrlbarUtils.HIGHLIGHT.TYPED],
            icon: [info.icon],
          })
        );
      default:
        Cu.reportError(`Unexpected action type: ${action.type}`);
        return null;
    }
  }

  if (info.style.includes("priority-search")) {
    return new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...UrlbarResult.payloadAndSimpleHighlights(tokens, {
        engine: [info.comment, UrlbarUtils.HIGHLIGHT.TYPED],
        icon: [info.icon],
      })
    );
  }

  // This is a normal url/title tuple.
  let source;
  let tags = [];
  let comment = info.comment;

  // UnifiedComplete may return "bookmark", "bookmark-tag" or "tag". In the last
  // case it should not be considered a bookmark, but an history item with tags.
  // We don't show tags for non bookmarked items though.
  if (info.style.includes("bookmark")) {
    source = UrlbarUtils.RESULT_SOURCE.BOOKMARKS;
  } else if (info.style.includes("preloaded-top-sites")) {
    source = UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL;
  } else {
    source = UrlbarUtils.RESULT_SOURCE.HISTORY;
  }

  // If the style indicates that the result is tagged, then the tags are
  // included in the title, and we must extract them.
  if (info.style.includes("tag")) {
    [comment, tags] = info.comment.split(UrlbarUtils.TITLE_TAGS_SEPARATOR);

    // However, as mentioned above, we don't want to show tags for non-
    // bookmarked items, so we include tags in the final result only if it's
    // bookmarked, and we drop the tags otherwise.
    if (source != UrlbarUtils.RESULT_SOURCE.BOOKMARKS) {
      tags = "";
    }

    // Tags are separated by a comma and in a random order.
    // We should also just include tags that match the searchString.
    tags = tags
      .split(",")
      .map(t => t.trim())
      .filter(tag => {
        let lowerCaseTag = tag.toLocaleLowerCase();
        return tokens.some(token =>
          lowerCaseTag.includes(token.lowerCaseValue)
        );
      })
      .sort();
  }

  return new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    source,
    ...UrlbarResult.payloadAndSimpleHighlights(tokens, {
      url: [info.url, UrlbarUtils.HIGHLIGHT.TYPED],
      icon: [info.icon],
      title: [comment, UrlbarUtils.HIGHLIGHT.TYPED],
      tags: [tags, UrlbarUtils.HIGHLIGHT.TYPED],
    })
  );
}
