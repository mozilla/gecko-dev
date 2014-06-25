/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Constants

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const TOPIC_SHUTDOWN = "places-shutdown";
const TOPIC_PREFCHANGED = "nsPref:changed";

const DEFAULT_BEHAVIOR = 0;

const PREF_BRANCH = "browser.urlbar";

// Prefs are defined as [pref name, default value].
const PREF_ENABLED =            [ "autocomplete.enabled", true ];
const PREF_AUTOFILL =           [ "autoFill",             true ];
const PREF_AUTOFILL_TYPED =     [ "autoFill.typed",       true ];
const PREF_AUTOFILL_PRIORITY =  [ "autoFill.priority",    true ];
const PREF_DELAY =              [ "delay",                  50 ];
const PREF_BEHAVIOR =           [ "matchBehavior", MATCH_BOUNDARY_ANYWHERE ];
const PREF_DEFAULT_BEHAVIOR =   [ "default.behavior", DEFAULT_BEHAVIOR ];
const PREF_EMPTY_BEHAVIOR =     [ "default.behavior.emptyRestriction",
                                  Ci.mozIPlacesAutoComplete.BEHAVIOR_HISTORY |
                                  Ci.mozIPlacesAutoComplete.BEHAVIOR_TYPED ];
const PREF_FILTER_JS =          [ "filter.javascript",    true ];
const PREF_MAXRESULTS =         [ "maxRichResults",         25 ];
const PREF_RESTRICT_HISTORY =   [ "restrict.history",      "^" ];
const PREF_RESTRICT_BOOKMARKS = [ "restrict.bookmark",     "*" ];
const PREF_RESTRICT_TYPED =     [ "restrict.typed",        "~" ];
const PREF_RESTRICT_TAG =       [ "restrict.tag",          "+" ];
const PREF_RESTRICT_SWITCHTAB = [ "restrict.openpage",     "%" ];
const PREF_MATCH_TITLE =        [ "match.title",           "#" ];
const PREF_MATCH_URL =          [ "match.url",             "@" ];

// Match type constants.
// These indicate what type of search function we should be using.
const MATCH_ANYWHERE = Ci.mozIPlacesAutoComplete.MATCH_ANYWHERE;
const MATCH_BOUNDARY_ANYWHERE = Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY_ANYWHERE;
const MATCH_BOUNDARY = Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY;
const MATCH_BEGINNING = Ci.mozIPlacesAutoComplete.MATCH_BEGINNING;
const MATCH_BEGINNING_CASE_SENSITIVE = Ci.mozIPlacesAutoComplete.MATCH_BEGINNING_CASE_SENSITIVE;

// AutoComplete query type constants.
// Describes the various types of queries that we can process rows for.
const QUERYTYPE_KEYWORD       = 0;
const QUERYTYPE_FILTERED      = 1;
const QUERYTYPE_AUTOFILL_HOST = 2;
const QUERYTYPE_AUTOFILL_URL  = 3;

// This separator is used as an RTL-friendly way to split the title and tags.
// It can also be used by an nsIAutoCompleteResult consumer to re-split the
// "comment" back into the title and the tag.
const TITLE_TAGS_SEPARATOR = " \u2013 ";

// Telemetry probes.
const TELEMETRY_1ST_RESULT = "PLACES_AUTOCOMPLETE_1ST_RESULT_TIME_MS";

// The default frecency value used when inserting priority results.
const FRECENCY_PRIORITY_DEFAULT = 1000;

// Sqlite result row index constants.
const QUERYINDEX_QUERYTYPE     = 0;
const QUERYINDEX_URL           = 1;
const QUERYINDEX_TITLE         = 2;
const QUERYINDEX_ICONURL       = 3;
const QUERYINDEX_BOOKMARKED    = 4;
const QUERYINDEX_BOOKMARKTITLE = 5;
const QUERYINDEX_TAGS          = 6;
const QUERYINDEX_VISITCOUNT    = 7;
const QUERYINDEX_TYPED         = 8;
const QUERYINDEX_PLACEID       = 9;
const QUERYINDEX_SWITCHTAB     = 10;
const QUERYINDEX_FRECENCY      = 11;

// This SQL query fragment provides the following:
//   - whether the entry is bookmarked (QUERYINDEX_BOOKMARKED)
//   - the bookmark title, if it is a bookmark (QUERYINDEX_BOOKMARKTITLE)
//   - the tags associated with a bookmarked entry (QUERYINDEX_TAGS)
const SQL_BOOKMARK_TAGS_FRAGMENT = sql(
  "EXISTS(SELECT 1 FROM moz_bookmarks WHERE fk = h.id) AS bookmarked,",
  "( SELECT title FROM moz_bookmarks WHERE fk = h.id AND title NOTNULL",
    "ORDER BY lastModified DESC LIMIT 1",
  ") AS btitle,",
  "( SELECT GROUP_CONCAT(t.title, ',')",
    "FROM moz_bookmarks b",
    "JOIN moz_bookmarks t ON t.id = +b.parent AND t.parent = :parent",
    "WHERE b.fk = h.id",
  ") AS tags");

// TODO bug 412736: in case of a frecency tie, we might break it with h.typed
// and h.visit_count.  That is slower though, so not doing it yet...
const SQL_DEFAULT_QUERY = sql(
  "SELECT :query_type, h.url, h.title, f.url,", SQL_BOOKMARK_TAGS_FRAGMENT, ",",
         "h.visit_count, h.typed, h.id, t.open_count, h.frecency",
  "FROM moz_places h",
  "LEFT JOIN moz_favicons f ON f.id = h.favicon_id",
  "LEFT JOIN moz_openpages_temp t ON t.url = h.url",
  "WHERE h.frecency <> 0",
    "AND AUTOCOMPLETE_MATCH(:searchString, h.url,",
                           "IFNULL(btitle, h.title), tags,",
                           "h.visit_count, h.typed,",
                           "bookmarked, t.open_count,",
                           ":matchBehavior, :searchBehavior)",
    "/*CONDITIONS*/",
  "ORDER BY h.frecency DESC, h.id DESC",
  "LIMIT :maxResults");

// Enforce ignoring the visit_count index, since the frecency one is much
// faster in this case.  ANALYZE helps the query planner to figure out the
// faster path, but it may not have up-to-date information yet.
const SQL_HISTORY_QUERY = SQL_DEFAULT_QUERY.replace("/*CONDITIONS*/",
                                                    "AND +h.visit_count > 0", "g");

const SQL_BOOKMARK_QUERY = SQL_DEFAULT_QUERY.replace("/*CONDITIONS*/",
                                                     "AND bookmarked", "g");

const SQL_TAGS_QUERY = SQL_DEFAULT_QUERY.replace("/*CONDITIONS*/",
                                                 "AND tags NOTNULL", "g");

const SQL_TYPED_QUERY = SQL_DEFAULT_QUERY.replace("/*CONDITIONS*/",
                                                  "AND h.typed = 1", "g");

const SQL_SWITCHTAB_QUERY = sql(
  "SELECT :query_type, t.url, t.url, NULL, NULL, NULL, NULL, NULL, NULL, NULL,",
         "t.open_count, NULL",
  "FROM moz_openpages_temp t",
  "LEFT JOIN moz_places h ON h.url = t.url",
  "WHERE h.id IS NULL",
    "AND AUTOCOMPLETE_MATCH(:searchString, t.url, t.url, NULL,",
                            "NULL, NULL, NULL, t.open_count,",
                            ":matchBehavior, :searchBehavior)",
  "ORDER BY t.ROWID DESC",
  "LIMIT :maxResults");

const SQL_ADAPTIVE_QUERY = sql(
  "/* do not warn (bug 487789) */",
  "SELECT :query_type, h.url, h.title, f.url,", SQL_BOOKMARK_TAGS_FRAGMENT, ",",
         "h.visit_count, h.typed, h.id, t.open_count, h.frecency",
  "FROM (",
    "SELECT ROUND(MAX(use_count) * (1 + (input = :search_string)), 1) AS rank,",
           "place_id",
    "FROM moz_inputhistory",
    "WHERE input BETWEEN :search_string AND :search_string || X'FFFF'",
    "GROUP BY place_id",
  ") AS i",
  "JOIN moz_places h ON h.id = i.place_id",
  "LEFT JOIN moz_favicons f ON f.id = h.favicon_id",
  "LEFT JOIN moz_openpages_temp t ON t.url = h.url",
  "WHERE AUTOCOMPLETE_MATCH(NULL, h.url,",
                           "IFNULL(btitle, h.title), tags,",
                           "h.visit_count, h.typed, bookmarked,",
                           "t.open_count,",
                           ":matchBehavior, :searchBehavior)",
  "ORDER BY rank DESC, h.frecency DESC");

const SQL_KEYWORD_QUERY = sql(
  "/* do not warn (bug 487787) */",
  "SELECT :query_type,",
    "(SELECT REPLACE(url, '%s', :query_string) FROM moz_places WHERE id = b.fk)",
    "AS search_url, h.title,",
    "IFNULL(f.url, (SELECT f.url",
                   "FROM moz_places",
                   "JOIN moz_favicons f ON f.id = favicon_id",
                   "WHERE rev_host = (SELECT rev_host FROM moz_places WHERE id = b.fk)",
                   "ORDER BY frecency DESC",
                   "LIMIT 1)",
          "),",
    "1, b.title, NULL, h.visit_count, h.typed, IFNULL(h.id, b.fk),",
    "t.open_count, h.frecency",
  "FROM moz_keywords k",
  "JOIN moz_bookmarks b ON b.keyword_id = k.id",
  "LEFT JOIN moz_places h ON h.url = search_url",
  "LEFT JOIN moz_favicons f ON f.id = h.favicon_id",
  "LEFT JOIN moz_openpages_temp t ON t.url = search_url",
  "WHERE LOWER(k.keyword) = LOWER(:keyword)",
  "ORDER BY h.frecency DESC");

const SQL_HOST_QUERY = sql(
  "/* do not warn (bug NA): not worth to index on (typed, frecency) */",
  "SELECT :query_type, host || '/', prefix || host || '/',",
         "NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, frecency",
  "FROM moz_hosts",
  "WHERE host BETWEEN :searchString AND :searchString || X'FFFF'",
  "AND frecency <> 0",
  "/*CONDITIONS*/",
  "ORDER BY frecency DESC",
  "LIMIT 1");

const SQL_TYPED_HOST_QUERY = SQL_HOST_QUERY.replace("/*CONDITIONS*/",
                                                    "AND typed = 1");
const SQL_URL_QUERY = sql(
  "/* do not warn (bug no): cannot use an index */",
  "SELECT :query_type, h.url,",
         "NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, h.frecency",
  "FROM moz_places h",
  "WHERE h.frecency <> 0",
  "/*CONDITIONS*/",
  "AND AUTOCOMPLETE_MATCH(:searchString, h.url,",
  "h.title, '',",
  "h.visit_count, h.typed, 0, 0,",
  ":matchBehavior, :searchBehavior)",
  "ORDER BY h.frecency DESC, h.id DESC",
  "LIMIT 1");

const SQL_TYPED_URL_QUERY = SQL_URL_QUERY.replace("/*CONDITIONS*/",
                                                  "AND typed = 1");

////////////////////////////////////////////////////////////////////////////////
//// Getters

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch",
                                  "resource://gre/modules/TelemetryStopwatch.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PriorityUrlProvider",
                                  "resource://gre/modules/PriorityUrlProvider.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "textURIService",
                                   "@mozilla.org/intl/texttosuburi;1",
                                   "nsITextToSubURI");

/**
 * Storage object for switch-to-tab entries.
 * This takes care of caching and registering open pages, that will be reused
 * by switch-to-tab queries.  It has an internal cache, so that the Sqlite
 * store is lazy initialized only on first use.
 * It has a simple API:
 *   initDatabase(conn): initializes the temporary Sqlite entities to store data
 *   add(uri): adds a given nsIURI to the store
 *   delete(uri): removes a given nsIURI from the store
 *   shutdown(): stops storing data to Sqlite
 */
XPCOMUtils.defineLazyGetter(this, "SwitchToTabStorage", () => Object.seal({
  _conn: null,
  // Temporary queue used while the database connection is not available.
  _queue: new Set(),
  initDatabase: Task.async(function* (conn) {
    // To reduce IO use an in-memory table for switch-to-tab tracking.
    // Note: this should be kept up-to-date with the definition in
    //       nsPlacesTables.h.
    yield conn.execute(sql(
      "CREATE TEMP TABLE moz_openpages_temp (",
        "url TEXT PRIMARY KEY,",
        "open_count INTEGER",
      ")"));

    // Note: this should be kept up-to-date with the definition in
    //       nsPlacesTriggers.h.
    yield conn.execute(sql(
      "CREATE TEMPORARY TRIGGER moz_openpages_temp_afterupdate_trigger",
      "AFTER UPDATE OF open_count ON moz_openpages_temp FOR EACH ROW",
      "WHEN NEW.open_count = 0",
      "BEGIN",
        "DELETE FROM moz_openpages_temp",
        "WHERE url = NEW.url;",
      "END"));

    this._conn = conn;

    // Populate the table with the current cache contents...
    this._queue.forEach(this.add, this);
    // ...then clear it to avoid double additions.
    this._queue.clear();
  }),

  add: function (uri) {
    if (!this._conn) {
      this._queue.add(uri);
      return;
    }
    this._conn.executeCached(sql(
      "INSERT OR REPLACE INTO moz_openpages_temp (url, open_count)",
        "VALUES ( :url, IFNULL( (SELECT open_count + 1",
                                 "FROM moz_openpages_temp",
                                 "WHERE url = :url),",
                                 "1",
                             ")",
               ")"
    ), { url: uri.spec });
  },

  delete: function (uri) {
    if (!this._conn) {
      this._queue.delete(uri);
      return;
    }
    this._conn.executeCached(sql(
      "UPDATE moz_openpages_temp",
      "SET open_count = open_count - 1",
      "WHERE url = :url"
    ), { url: uri.spec });
  },

  shutdown: function () {
    this._conn = null;
    this._queue.clear();
  }
}));

/**
 * This helper keeps track of preferences and keeps their values up-to-date.
 */
XPCOMUtils.defineLazyGetter(this, "Prefs", () => {
  let prefs = new Preferences(PREF_BRANCH);

  function loadPrefs() {
    store.enabled = prefs.get(...PREF_ENABLED);
    store.autofill = prefs.get(...PREF_AUTOFILL);
    store.autofillTyped = prefs.get(...PREF_AUTOFILL_TYPED);
    store.autofillPriority = prefs.get(...PREF_AUTOFILL_PRIORITY);
    store.delay = prefs.get(...PREF_DELAY);
    store.matchBehavior = prefs.get(...PREF_BEHAVIOR);
    store.filterJavaScript = prefs.get(...PREF_FILTER_JS);
    store.maxRichResults = prefs.get(...PREF_MAXRESULTS);
    store.restrictHistoryToken = prefs.get(...PREF_RESTRICT_HISTORY);
    store.restrictBookmarkToken = prefs.get(...PREF_RESTRICT_BOOKMARKS);
    store.restrictTypedToken = prefs.get(...PREF_RESTRICT_TYPED);
    store.restrictTagToken = prefs.get(...PREF_RESTRICT_TAG);
    store.restrictOpenPageToken = prefs.get(...PREF_RESTRICT_SWITCHTAB);
    store.matchTitleToken = prefs.get(...PREF_MATCH_TITLE);
    store.matchURLToken = prefs.get(...PREF_MATCH_URL);
    store.defaultBehavior = prefs.get(...PREF_DEFAULT_BEHAVIOR);
    // Further restrictions to apply for "empty searches" (i.e. searches for "").
    store.emptySearchDefaultBehavior = store.defaultBehavior |
                                       prefs.get(...PREF_EMPTY_BEHAVIOR);

    // Validate matchBehavior; default to MATCH_BOUNDARY_ANYWHERE.
    if (store.matchBehavior != MATCH_ANYWHERE &&
        store.matchBehavior != MATCH_BOUNDARY &&
        store.matchBehavior != MATCH_BEGINNING) {
      store.matchBehavior = MATCH_BOUNDARY_ANYWHERE;
    }

    store.tokenToBehaviorMap = new Map([
      [ store.restrictHistoryToken, "history" ],
      [ store.restrictBookmarkToken, "bookmark" ],
      [ store.restrictTagToken, "tag" ],
      [ store.restrictOpenPageToken, "openpage" ],
      [ store.matchTitleToken, "title" ],
      [ store.matchURLToken, "url" ],
      [ store.restrictTypedToken, "typed" ]
    ]);
  }

  let store = {
    observe: function (subject, topic, data) {
      loadPrefs();
    },
    QueryInterface: XPCOMUtils.generateQI([ Ci.nsIObserver ])
  };
  loadPrefs();
  prefs.observe("", store);

  return Object.seal(store);
});

////////////////////////////////////////////////////////////////////////////////
//// Helper functions

/**
 * Joins multiple sql tokens into a single sql query.
 */
function sql(...parts) parts.join(" ");

/**
 * Used to unescape encoded URI strings and drop information that we do not
 * care about.
 *
 * @param spec
 *        The text to unescape and modify.
 * @return the modified spec.
 */
function fixupSearchText(spec)
  textURIService.unEscapeURIForUI("UTF-8", stripPrefix(spec));

/**
 * Generates the tokens used in searching from a given string.
 *
 * @param searchString
 *        The string to generate tokens from.
 * @return an array of tokens.
 * @note Calling split on an empty string will return an array containing one
 *       empty string.  We don't want that, as it'll break our logic, so return
 *       an empty array then.
 */
function getUnfilteredSearchTokens(searchString)
  searchString.length ? searchString.split(" ") : [];

/**
 * Strip prefixes from the URI that we don't care about for searching.
 *
 * @param spec
 *        The text to modify.
 * @return the modified spec.
 */
function stripPrefix(spec)
{
  ["http://", "https://", "ftp://"].some(scheme => {
    if (spec.startsWith(scheme)) {
      spec = spec.slice(scheme.length);
      return true;
    }
    return false;
  });

  if (spec.startsWith("www.")) {
    spec = spec.slice(4);
  }
  return spec;
}

////////////////////////////////////////////////////////////////////////////////
//// Search Class
//// Manages a single instance of an autocomplete search.

function Search(searchString, searchParam, autocompleteListener,
                resultListener, autocompleteSearch) {
  // We want to store the original string with no leading or trailing
  // whitespace for case sensitive searches.
  this._originalSearchString = searchString.trim();
  this._searchString = fixupSearchText(this._originalSearchString.toLowerCase());
  this._searchTokens =
    this.filterTokens(getUnfilteredSearchTokens(this._searchString));
  // The protocol and the host are lowercased by nsIURI, so it's fine to
  // lowercase the typed prefix, to add it back to the results later.
  this._strippedPrefix = this._originalSearchString.slice(
    0, this._originalSearchString.length - this._searchString.length
  ).toLowerCase();
  // The URIs in the database are fixed-up, so we can match on a lowercased
  // host, but the path must be matched in a case sensitive way.
  let pathIndex =
    this._originalSearchString.indexOf("/", this._strippedPrefix.length);
  this._autofillUrlSearchString = fixupSearchText(
    this._originalSearchString.slice(0, pathIndex).toLowerCase() +
    this._originalSearchString.slice(pathIndex)
  );

  this._enableActions = searchParam.split(" ").indexOf("enable-actions") != -1;

  this._listener = autocompleteListener;
  this._autocompleteSearch = autocompleteSearch;

  this._matchBehavior = Prefs.matchBehavior;
  // Set the default behavior for this search.
  this._behavior = this._searchString ? Prefs.defaultBehavior
                                      : Prefs.emptySearchDefaultBehavior;
  // Create a new result to add eventual matches.  Note we need a result
  // regardless having matches.
  let result = Cc["@mozilla.org/autocomplete/simple-result;1"]
                 .createInstance(Ci.nsIAutoCompleteSimpleResult);
  result.setSearchString(searchString);
  result.setListener(resultListener);
  // Will be set later, if needed.
  result.setDefaultIndex(-1);
  this._result = result;

  // These are used to avoid adding duplicate entries to the results.
  this._usedURLs = new Set();
  this._usedPlaceIds = new Set();
}

Search.prototype = {
  /**
   * Enables the desired AutoComplete behavior.
   *
   * @param type
   *        The behavior type to set.
   */
  setBehavior: function (type) {
    this._behavior |=
      Ci.mozIPlacesAutoComplete["BEHAVIOR_" + type.toUpperCase()];
  },

  /**
   * Determines if the specified AutoComplete behavior is set.
   *
   * @param aType
   *        The behavior type to test for.
   * @return true if the behavior is set, false otherwise.
   */
  hasBehavior: function (type) {
    return this._behavior &
           Ci.mozIPlacesAutoComplete["BEHAVIOR_" + type.toUpperCase()];
  },

  /**
   * Used to delay the most complex queries, to save IO while the user is
   * typing.
   */
  _sleepDeferred: null,
  _sleep: function (aTimeMs) {
    // Reuse a single instance to try shaving off some usless work before
    // the first query.
    if (!this._sleepTimer)
      this._sleepTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._sleepDeferred = Promise.defer();
    this._sleepTimer.initWithCallback(() => this._sleepDeferred.resolve(),
                                      aTimeMs, Ci.nsITimer.TYPE_ONE_SHOT);
    return this._sleepDeferred.promise;
  },

  /**
   * Given an array of tokens, this function determines which query should be
   * ran.  It also removes any special search tokens.
   *
   * @param tokens
   *        An array of search tokens.
   * @return the filtered list of tokens to search with.
   */
  filterTokens: function (tokens) {
    // Set the proper behavior while filtering tokens.
    for (let i = tokens.length - 1; i >= 0; i--) {
      let behavior = Prefs.tokenToBehaviorMap.get(tokens[i]);
      // Don't remove the token if it didn't match, or if it's an action but
      // actions are not enabled.
      if (behavior && (behavior != "openpage" || this._enableActions)) {
        this.setBehavior(behavior);
        tokens.splice(i, 1);
      }
    }

    // Set the right JavaScript behavior based on our preference.  Note that the
    // preference is whether or not we should filter JavaScript, and the
    // behavior is if we should search it or not.
    if (!Prefs.filterJavaScript) {
      this.setBehavior("javascript");
    }

    return tokens;
  },

  /**
   * Used to cancel this search, will stop providing results.
   */
  cancel: function () {
    if (this._sleepTimer)
      this._sleepTimer.cancel();
    if (this._sleepDeferred) {
      this._sleepDeferred.resolve();
      this._sleepDeferred = null;
    }
    delete this._pendingQuery;
  },

  /**
   * Whether this search is running.
   */
  get pending() !!this._pendingQuery,

  /**
   * Execute the search and populate results.
   * @param conn
   *        The Sqlite connection.
   */
  execute: Task.async(function* (conn) {
    this._pendingQuery = true;
    TelemetryStopwatch.start(TELEMETRY_1ST_RESULT);

    // For any given search, we run many queries:
    // 1) priority domains
    // 2) inline completion
    // 3) keywords (this._keywordQuery)
    // 4) adaptive learning (this._adaptiveQuery)
    // 5) open pages not supported by history (this._switchToTabQuery)
    // 6) query based on match behavior
    //
    // (3) only gets ran if we get any filtered tokens, since if there are no
    // tokens, there is nothing to match.

    // Get the final query, based on the tokens found in the search string.
    let queries = [ this._adaptiveQuery,
                    this._switchToTabQuery,
                    this._searchQuery ];

    if (this._searchTokens.length == 1) {
      yield this._matchPriorityUrl();
    } else if (this._searchTokens.length > 1) {
      queries.unshift(this._keywordQuery);
    }

    if (this._shouldAutofill) {
      // Hosts have no "/" in them.
      let lastSlashIndex = this._searchString.lastIndexOf("/");
      // Search only URLs if there's a slash in the search string...
      if (lastSlashIndex != -1) {
        // ...but not if it's exactly at the end of the search string.
        if (lastSlashIndex < this._searchString.length - 1) {
          queries.unshift(this._urlQuery);
        }
      } else if (this.pending) {
        // The host query is executed immediately, while any other is delayed
        // to avoid overloading the connection.
        let [ query, params ] = this._hostQuery;
        yield conn.executeCached(query, params, this._onResultRow.bind(this));
      }
    }

    yield this._sleep(Prefs.delay);
    if (!this.pending)
      return;

    for (let [query, params] of queries) {
      yield conn.executeCached(query, params, this._onResultRow.bind(this));
      if (!this.pending)
        return;
    }

    // If we do not have enough results, and our match type is
    // MATCH_BOUNDARY_ANYWHERE, search again with MATCH_ANYWHERE to get more
    // results.
    if (this._matchBehavior == MATCH_BOUNDARY_ANYWHERE &&
        this._result.matchCount < Prefs.maxRichResults) {
      this._matchBehavior = MATCH_ANYWHERE;
      for (let [query, params] of [ this._adaptiveQuery,
                                    this._searchQuery ]) {
        yield conn.executeCached(query, params, this._onResultRow);
        if (!this.pending)
          return;
      }
    }

    // If we didn't find enough matches and we have some frecency-driven
    // matches, add them.
    if (this._frecencyMatches) {
      this._frecencyMatches.forEach(this._addMatch, this);
    }
  }),

  _matchPriorityUrl: function* () {
    if (!Prefs.autofillPriority)
      return;
    let priorityMatch = yield PriorityUrlProvider.getMatch(this._searchString);
    if (priorityMatch) {
      this._result.setDefaultIndex(0);
      this._addFrecencyMatch({
        value: priorityMatch.token,
        comment: priorityMatch.title,
        icon: priorityMatch.iconUrl,
        style: "priority-" + priorityMatch.reason,
        finalCompleteValue: priorityMatch.url,
        frecency: FRECENCY_PRIORITY_DEFAULT
      });
    }
  },

  _onResultRow: function (row) {
    TelemetryStopwatch.finish(TELEMETRY_1ST_RESULT);
    let queryType = row.getResultByIndex(QUERYINDEX_QUERYTYPE);
    let match;
    switch (queryType) {
      case QUERYTYPE_AUTOFILL_HOST:
        this._result.setDefaultIndex(0);
        match = this._processHostRow(row);
        break;
      case QUERYTYPE_AUTOFILL_URL:
        this._result.setDefaultIndex(0);
        match = this._processUrlRow(row);
        break;
      case QUERYTYPE_FILTERED:
      case QUERYTYPE_KEYWORD:
        match = this._processRow(row);
        break;
    }
    this._addMatch(match);
  },

  /**
   * These matches should be mixed up with other matches, based on frecency.
   */
  _addFrecencyMatch: function (match) {
    if (!this._frecencyMatches)
      this._frecencyMatches = [];
    this._frecencyMatches.push(match);
    // We keep this array in reverse order, so we can walk it and remove stuff
    // from it in one pass.  Notice that for frecency reverse order means from
    // lower to higher.
    this._frecencyMatches.sort((a, b) => a.frecency - b.frecency);
  },

  _addMatch: function (match) {
    let notifyResults = false;

    if (this._frecencyMatches) {
      for (let i = this._frecencyMatches.length - 1;  i >= 0 ; i--) {
        if (this._frecencyMatches[i].frecency > match.frecency) {
          this._addMatch(this._frecencyMatches.splice(i, 1)[0]);
        }
      }
    }

    // Must check both id and url, cause keywords dinamically modify the url.
    if ((!match.placeId || !this._usedPlaceIds.has(match.placeId)) &&
        !this._usedURLs.has(stripPrefix(match.value))) {
      // Add this to our internal tracker to ensure duplicates do not end up in
      // the result.
      // Not all entries have a place id, thus we fallback to the url for them.
      // We cannot use only the url since keywords entries are modified to
      // include the search string, and would be returned multiple times.  Ids
      // are faster too.
      if (match.placeId)
        this._usedPlaceIds.add(match.placeId);
      this._usedURLs.add(stripPrefix(match.value));

      this._result.appendMatch(match.value,
                               match.comment,
                               match.icon || PlacesUtils.favicons.defaultFavicon.spec,
                               match.style || "favicon",
                               match.finalCompleteValue);
      notifyResults = true;
    }

    if (this._result.matchCount == Prefs.maxRichResults || !this.pending) {
      // We have enough results, so stop running our search.
      this.cancel();
      // This tells Sqlite.jsm to stop providing us results and cancel the
      // underlying query.
      throw StopIteration;
    }

    if (notifyResults) {
      // Notify about results if we've gotten them.
      this.notifyResults(true);
    }
  },

  _processHostRow: function (row) {
    let match = {};
    let trimmedHost = row.getResultByIndex(QUERYINDEX_URL);
    let untrimmedHost = row.getResultByIndex(QUERYINDEX_TITLE);
    let frecency = row.getResultByIndex(QUERYINDEX_FRECENCY);
    // If the untrimmed value doesn't preserve the user's input just
    // ignore it and complete to the found host.
    if (untrimmedHost &&
        !untrimmedHost.toLowerCase().contains(this._originalSearchString.toLowerCase())) {
      // THIS CAUSES null TO BE SHOWN AS TITLE.
      untrimmedHost = null;
    }

    match.value = this._strippedPrefix + trimmedHost;
    match.comment = trimmedHost;
    match.finalCompleteValue = untrimmedHost;
    match.frecency = frecency;
    return match;
  },

  _processUrlRow: function (row) {
    let match = {};
    let value = row.getResultByIndex(QUERYINDEX_URL);
    let url = fixupSearchText(value);
    let frecency = row.getResultByIndex(QUERYINDEX_FRECENCY);

    let prefix = value.slice(0, value.length - stripPrefix(value).length);

    // We must complete the URL up to the next separator (which is /, ? or #).
    let separatorIndex = url.slice(this._searchString.length)
                            .search(/[\/\?\#]/);
    if (separatorIndex != -1) {
      separatorIndex += this._searchString.length;
      if (url[separatorIndex] == "/") {
        separatorIndex++; // Include the "/" separator
      }
      url = url.slice(0, separatorIndex);
    }

    // If the untrimmed value doesn't preserve the user's input just
    // ignore it and complete to the found url.
    let untrimmedURL = prefix + url;
    if (untrimmedURL &&
        !untrimmedURL.toLowerCase().contains(this._originalSearchString.toLowerCase())) {
      // THIS CAUSES null TO BE SHOWN AS TITLE.
      untrimmedURL = null;
     }

    match.value = this._strippedPrefix + url;
    match.comment = url;
    match.finalCompleteValue = untrimmedURL;
    match.frecency = frecency;
    return match;
  },

  _processRow: function (row) {
    let match = {};
    match.placeId = row.getResultByIndex(QUERYINDEX_PLACEID);
    let queryType = row.getResultByIndex(QUERYINDEX_QUERYTYPE);
    let escapedURL = row.getResultByIndex(QUERYINDEX_URL);
    let openPageCount = row.getResultByIndex(QUERYINDEX_SWITCHTAB) || 0;
    let historyTitle = row.getResultByIndex(QUERYINDEX_TITLE) || "";
    let iconurl = row.getResultByIndex(QUERYINDEX_ICONURL) || "";
    let bookmarked = row.getResultByIndex(QUERYINDEX_BOOKMARKED);
    let bookmarkTitle = bookmarked ?
      row.getResultByIndex(QUERYINDEX_BOOKMARKTITLE) : null;
    let tags = row.getResultByIndex(QUERYINDEX_TAGS) || "";
    let frecency = row.getResultByIndex(QUERYINDEX_FRECENCY);

    // If actions are enabled and the page is open, add only the switch-to-tab
    // result.  Otherwise, add the normal result.
    let [url, action] = this._enableActions && openPageCount > 0 ?
                        ["moz-action:switchtab," + escapedURL, "action "] :
                        [escapedURL, ""];

    // Always prefer the bookmark title unless it is empty
    let title = bookmarkTitle || historyTitle;

    if (queryType == QUERYTYPE_KEYWORD) {
      // If we do not have a title, then we must have a keyword, so let the UI
      // know it is a keyword.  Otherwise, we found an exact page match, so just
      // show the page like a regular result.  Because the page title is likely
      // going to be more specific than the bookmark title (keyword title).
      if (!historyTitle) {
        match.style = "keyword";
      }
      else {
        title = historyTitle;
      }
    }

    // We will always prefer to show tags if we have them.
    let showTags = !!tags;

    // However, we'll act as if a page is not bookmarked or tagged if the user
    // only wants only history and not bookmarks or tags.
    if (this.hasBehavior("history") &&
        !(this.hasBehavior("bookmark") || this.hasBehavior("tag"))) {
      showTags = false;
      match.style = "favicon";
    }

    // If we have tags and should show them, we need to add them to the title.
    if (showTags) {
      title += TITLE_TAGS_SEPARATOR + tags;
    }

    // We have to determine the right style to display.  Tags show the tag icon,
    // bookmarks get the bookmark icon, and keywords get the keyword icon.  If
    // the result does not fall into any of those, it just gets the favicon.
    if (!match.style) {
      // It is possible that we already have a style set (from a keyword
      // search or because of the user's preferences), so only set it if we
      // haven't already done so.
      if (showTags) {
        match.style = "tag";
      }
      else if (bookmarked) {
        match.style = "bookmark";
      }
    }

    match.value = url;
    match.comment = title;
    if (iconurl) {
      match.icon = PlacesUtils.favicons
                              .getFaviconLinkForIcon(NetUtil.newURI(iconurl)).spec;
    }
    match.frecency = frecency;

    return match;
  },

  /**
   * Obtains the search query to be used based on the previously set search
   * behaviors (accessed by this.hasBehavior).
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _searchQuery() {
    // We use more optimized queries for restricted searches, so we will always
    // return the most restrictive one to the least restrictive one if more than
    // one token is found.
    // Note: "openpages" behavior is supported by the default query.
    //       _switchToTabQuery instead returns only pages not supported by
    //       history and it is always executed.
    let query = this.hasBehavior("tag") ? SQL_TAGS_QUERY :
                this.hasBehavior("bookmark") ? SQL_BOOKMARK_QUERY :
                this.hasBehavior("typed") ? SQL_TYPED_QUERY :
                this.hasBehavior("history") ? SQL_HISTORY_QUERY :
                SQL_DEFAULT_QUERY;

    return [
      query,
      {
        parent: PlacesUtils.tagsFolderId,
        query_type: QUERYTYPE_FILTERED,
        matchBehavior: this._matchBehavior,
        searchBehavior: this._behavior,
        // We only want to search the tokens that we are left with - not the
        // original search string.
        searchString: this._searchTokens.join(" "),
        // Limit the query to the the maximum number of desired results.
        // This way we can avoid doing more work than needed.
        maxResults: Prefs.maxRichResults
      }
    ];
  },

  /**
   * Obtains the query to search for keywords.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _keywordQuery() {
    // The keyword is the first word in the search string, with the parameters
    // following it.
    let searchString = this._originalSearchString;
    let queryString = "";
    let queryIndex = searchString.indexOf(" ");
    if (queryIndex != -1) {
      queryString = searchString.substring(queryIndex + 1);
    }
    // We need to escape the parameters as if they were the query in a URL
    queryString = encodeURIComponent(queryString).replace("%20", "+", "g");

    // The first word could be a keyword, so that's what we'll search.
    let keyword = this._searchTokens[0];

    return [
      SQL_KEYWORD_QUERY,
      {
        keyword: keyword,
        query_string: queryString,
        query_type: QUERYTYPE_KEYWORD
      }
    ];
  },

  /**
   * Obtains the query to search for switch-to-tab entries.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _switchToTabQuery() [
    SQL_SWITCHTAB_QUERY,
    {
      query_type: QUERYTYPE_FILTERED,
      matchBehavior: this._matchBehavior,
      searchBehavior: this._behavior,
      // We only want to search the tokens that we are left with - not the
      // original search string.
      searchString: this._searchTokens.join(" "),
      maxResults: Prefs.maxRichResults
    }
  ],

  /**
   * Obtains the query to search for adaptive results.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _adaptiveQuery() [
    SQL_ADAPTIVE_QUERY,
    {
      parent: PlacesUtils.tagsFolderId,
      search_string: this._searchString,
      query_type: QUERYTYPE_FILTERED,
      matchBehavior: this._matchBehavior,
      searchBehavior: this._behavior
    }
  ],

  /**
   * Whether we should try to autoFill.
   */
  get _shouldAutofill() {
    // First of all, check for the autoFill pref.
    if (!Prefs.autofill)
      return false;

    // Then, we should not try to autofill if the behavior is not the default.
    // TODO (bug 751709): Ideally we should have a more fine-grained behavior
    // here, but for now it's enough to just check for default behavior.
    if (Prefs.defaultBehavior != DEFAULT_BEHAVIOR)
      return false;

    // Don't autoFill if the search term is recognized as a keyword, otherwise
    // it will override default keywords behavior.  Note that keywords are
    // hashed on first use, so while the first query may delay a little bit,
    // next ones will just hit the memory hash.
    if (this._searchString.length == 0 ||
        PlacesUtils.bookmarks.getURIForKeyword(this._searchString)) {
      return false;
    }

    // Don't try to autofill if the search term includes any whitespace.
    // This may confuse completeDefaultIndex cause the AUTOCOMPLETE_MATCH
    // tokenizer ends up trimming the search string and returning a value
    // that doesn't match it, or is even shorter.
    if (/\s/.test(this._searchString)) {
      return false;
    }

    return true;
  },

  /**
   * Obtains the query to search for autoFill host results.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _hostQuery() [
    Prefs.autofillTyped ? SQL_TYPED_HOST_QUERY : SQL_TYPED_QUERY,
    {
      query_type: QUERYTYPE_AUTOFILL_HOST,
      searchString: this._searchString.toLowerCase()
    }
  ],

  /**
   * Obtains the query to search for autoFill url results.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _urlQuery() [
    Prefs.autofillTyped ? SQL_TYPED_HOST_QUERY : SQL_TYPED_QUERY,
    {
      query_type: QUERYTYPE_AUTOFILL_URL,
      searchString: this._autofillUrlSearchString,
      matchBehavior: MATCH_BEGINNING_CASE_SENSITIVE,
      searchBehavior: Ci.mozIPlacesAutoComplete.BEHAVIOR_URL
    }
  ],

 /**
   * Notifies the listener about results.
   *
   * @param searchOngoing
   *        Indicates whether the search is ongoing.
   */
  notifyResults: function (searchOngoing) {
    let result = this._result;
    let resultCode = result.matchCount ? "RESULT_SUCCESS" : "RESULT_NOMATCH";
    if (searchOngoing) {
      resultCode += "_ONGOING";
    }
    result.setSearchResult(Ci.nsIAutoCompleteResult[resultCode]);
    this._listener.onSearchResult(this._autocompleteSearch, result);
  },
}

////////////////////////////////////////////////////////////////////////////////
//// UnifiedComplete class
//// component @mozilla.org/autocomplete/search;1?name=unifiedcomplete

function UnifiedComplete() {
  Services.obs.addObserver(this, TOPIC_SHUTDOWN, true);
}

UnifiedComplete.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function (subject, topic, data) {
    if (topic === TOPIC_SHUTDOWN) {
      this.ensureShutdown();
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Database handling

  /**
   * Promise resolved when the database initialization has completed, or null
   * if it has never been requested.
   */
  _promiseDatabase: null,

  /**
   * Gets a Sqlite database handle.
   *
   * @return {Promise}
   * @resolves to the Sqlite database handle (according to Sqlite.jsm).
   * @rejects javascript exception.
   */
  getDatabaseHandle: function () {
    if (Prefs.enabled && !this._promiseDatabase) {
      this._promiseDatabase = Task.spawn(function* () {
        let conn = yield Sqlite.cloneStorageConnection({
          connection: PlacesUtils.history.DBConnection,
          readOnly: true
        });

        // Autocomplete often fallbacks to a table scan due to lack of text
        // indices.  A larger cache helps reducing IO and improving performance.
        // The value used here is larger than the default Storage value defined
        // as MAX_CACHE_SIZE_BYTES in storage/src/mozStorageConnection.cpp.
        yield conn.execute("PRAGMA cache_size = -6144"); // 6MiB

        yield SwitchToTabStorage.initDatabase(conn);

        return conn;
      }.bind(this)).then(null, Cu.reportError);
    }
    return this._promiseDatabase;
  },

  /**
   * Used to stop running queries and close the database handle.
   */
  ensureShutdown: function () {
    if (this._promiseDatabase) {
      Task.spawn(function* () {
        let conn = yield this.getDatabaseHandle();
        SwitchToTabStorage.shutdown();
        yield conn.close()
      }.bind(this)).then(null, Cu.reportError);
      this._promiseDatabase = null;
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// mozIPlacesAutoComplete

  registerOpenPage: function PAC_registerOpenPage(uri) {
    SwitchToTabStorage.add(uri);
  },

  unregisterOpenPage: function PAC_unregisterOpenPage(uri) {
    SwitchToTabStorage.delete(uri);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIAutoCompleteSearch

  startSearch: function (searchString, searchParam, previousResult, listener) {
    // Stop the search in case the controller has not taken care of it.
    if (this._currentSearch) {
      this.stopSearch();
    }

    // Note: We don't use previousResult to make sure ordering of results are
    //       consistent.  See bug 412730 for more details.

    this._currentSearch = new Search(searchString, searchParam, listener,
                                     this, this);

    // If we are not enabled, we need to return now.  Notice we need an empty
    // result regardless, so we still create the Search object.
    if (!Prefs.enabled) {
      this.finishSearch(true);
      return;
    }

    let search = this._currentSearch;
    this.getDatabaseHandle().then(conn => search.execute(conn))
                            .then(() => {
                              if (search == this._currentSearch) {
                                this.finishSearch(true);
                              }
                            }, Cu.reportError);
  },

  stopSearch: function () {
    if (this._currentSearch) {
      this._currentSearch.cancel();
    }
    this.finishSearch();
  },

  /**
   * Properly cleans up when searching is completed.
   *
   * @param notify [optional]
   *        Indicates if we should notify the AutoComplete listener about our
   *        results or not.
   */
  finishSearch: function (notify=false) {
    // Notify about results if we are supposed to.
    if (notify) {
      this._currentSearch.notifyResults(false);
    }

    // Clear our state
    TelemetryStopwatch.cancel(TELEMETRY_1ST_RESULT);
    delete this._currentSearch;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIAutoCompleteSimpleResultListener

  onValueRemoved: function (result, spec, removeFromDB) {
    if (removeFromDB) {
      PlacesUtils.history.removePage(NetUtil.newURI(spec));
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIAutoCompleteSearchDescriptor

  get searchType() Ci.nsIAutoCompleteSearchDescriptor.SEARCH_TYPE_IMMEDIATE,

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  classID: Components.ID("f964a319-397a-4d21-8be6-5cdd1ee3e3ae"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(UnifiedComplete),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIAutoCompleteSearch,
    Ci.nsIAutoCompleteSimpleResultListener,
    Ci.mozIPlacesAutoComplete,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference
  ])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([UnifiedComplete]);
