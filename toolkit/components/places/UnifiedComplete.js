/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint complexity: ["error", 50] */

"use strict";

// Constants

const MS_PER_DAY = 86400000; // 24 * 60 * 60 * 1000

// AutoComplete query type constants.
// Describes the various types of queries that we can process rows for.
const QUERYTYPE_FILTERED            = 0;
const QUERYTYPE_AUTOFILL_ORIGIN     = 1;
const QUERYTYPE_AUTOFILL_URL        = 2;
const QUERYTYPE_ADAPTIVE            = 3;

// This separator is used as an RTL-friendly way to split the title and tags.
// It can also be used by an nsIAutoCompleteResult consumer to re-split the
// "comment" back into the title and the tag.
const TITLE_TAGS_SEPARATOR = " \u2013 ";

// Telemetry probes.
const TELEMETRY_1ST_RESULT = "PLACES_AUTOCOMPLETE_1ST_RESULT_TIME_MS";
const TELEMETRY_6_FIRST_RESULTS = "PLACES_AUTOCOMPLETE_6_FIRST_RESULTS_TIME_MS";
// The default frecency value used when inserting matches with unknown frecency.
const FRECENCY_DEFAULT = 1000;

// After this time, we'll give up waiting for the extension to return matches.
const MAXIMUM_ALLOWED_EXTENSION_TIME_MS = 3000;

// By default we add remote tabs that have been used less than this time ago.
// Any remaining remote tabs are added in queue if no other results are found.
const RECENT_REMOTE_TAB_THRESHOLD_MS = 259200000; // 72 hours.

// A regex that matches "single word" hostnames for whitelisting purposes.
// The hostname will already have been checked for general validity, so we
// don't need to be exhaustive here, so allow dashes anywhere.
const REGEXP_SINGLEWORD_HOST = new RegExp("^[a-z0-9-]+$", "i");

// Regex used to match userContextId.
const REGEXP_USER_CONTEXT_ID = /(?:^| )user-context-id:(\d+)/;

// Regex used to match one or more whitespace.
const REGEXP_SPACES = /\s+/;

// Regex used to strip prefixes from URLs.  See stripPrefix().
const REGEXP_STRIP_PREFIX = /^[a-zA-Z]+:(?:\/\/)?/;

// Cannot contains spaces or path delims.
const REGEXP_ORIGIN = /^[^\s\/\?\#]+$/;

// The result is notified on a delay, to avoid rebuilding the panel at every match.
const NOTIFYRESULT_DELAY_MS = 16;

// Sqlite result row index constants.
const QUERYINDEX_QUERYTYPE     = 0;
const QUERYINDEX_URL           = 1;
const QUERYINDEX_TITLE         = 2;
const QUERYINDEX_BOOKMARKED    = 3;
const QUERYINDEX_BOOKMARKTITLE = 4;
const QUERYINDEX_TAGS          = 5;
const QUERYINDEX_VISITCOUNT    = 6;
const QUERYINDEX_TYPED         = 7;
const QUERYINDEX_PLACEID       = 8;
const QUERYINDEX_SWITCHTAB     = 9;
const QUERYINDEX_FRECENCY      = 10;

// If a URL starts with one of these prefixes, then we don't provide search
// suggestions for it.
const DISALLOWED_URLLIKE_PREFIXES = [
  "http", "https", "ftp",
];

// This SQL query fragment provides the following:
//   - whether the entry is bookmarked (QUERYINDEX_BOOKMARKED)
//   - the bookmark title, if it is a bookmark (QUERYINDEX_BOOKMARKTITLE)
//   - the tags associated with a bookmarked entry (QUERYINDEX_TAGS)
const SQL_BOOKMARK_TAGS_FRAGMENT =
  `EXISTS(SELECT 1 FROM moz_bookmarks WHERE fk = h.id) AS bookmarked,
   ( SELECT title FROM moz_bookmarks WHERE fk = h.id AND title NOTNULL
     ORDER BY lastModified DESC LIMIT 1
   ) AS btitle,
   ( SELECT GROUP_CONCAT(t.title, ', ')
     FROM moz_bookmarks b
     JOIN moz_bookmarks t ON t.id = +b.parent AND t.parent = :parent
     WHERE b.fk = h.id
   ) AS tags`;

// TODO bug 412736: in case of a frecency tie, we might break it with h.typed
// and h.visit_count.  That is slower though, so not doing it yet...
// NB: as a slight performance optimization, we only evaluate the "bookmarked"
// condition once, and avoid evaluating "btitle" and "tags" when it is false.
function defaultQuery(conditions = "") {
  let query =
    `SELECT :query_type, h.url, h.title, ${SQL_BOOKMARK_TAGS_FRAGMENT},
            h.visit_count, h.typed, h.id, t.open_count, h.frecency
     FROM moz_places h
     LEFT JOIN moz_openpages_temp t
            ON t.url = h.url
           AND t.userContextId = :userContextId
     WHERE h.frecency <> 0
       AND CASE WHEN bookmarked
         THEN
           AUTOCOMPLETE_MATCH(:searchString, h.url,
                              IFNULL(btitle, h.title), tags,
                              h.visit_count, h.typed,
                              1, t.open_count,
                              :matchBehavior, :searchBehavior)
         ELSE
           AUTOCOMPLETE_MATCH(:searchString, h.url,
                              h.title, '',
                              h.visit_count, h.typed,
                              0, t.open_count,
                              :matchBehavior, :searchBehavior)
         END
       ${conditions}
     ORDER BY h.frecency DESC, h.id DESC
     LIMIT :maxResults`;
  return query;
}

const SQL_SWITCHTAB_QUERY =
  `SELECT :query_type, t.url, t.url, NULL, NULL, NULL, NULL, NULL, NULL,
          t.open_count, NULL
   FROM moz_openpages_temp t
   LEFT JOIN moz_places h ON h.url_hash = hash(t.url) AND h.url = t.url
   WHERE h.id IS NULL
     AND t.userContextId = :userContextId
     AND AUTOCOMPLETE_MATCH(:searchString, t.url, t.url, NULL,
                            NULL, NULL, NULL, t.open_count,
                            :matchBehavior, :searchBehavior)
   ORDER BY t.ROWID DESC
   LIMIT :maxResults`;

const SQL_ADAPTIVE_QUERY =
  `/* do not warn (bug 487789) */
   SELECT :query_type, h.url, h.title, ${SQL_BOOKMARK_TAGS_FRAGMENT},
          h.visit_count, h.typed, h.id, t.open_count, h.frecency
   FROM (
     SELECT ROUND(MAX(use_count) * (1 + (input = :search_string)), 1) AS rank,
            place_id
     FROM moz_inputhistory
     WHERE input BETWEEN :search_string AND :search_string || X'FFFF'
     GROUP BY place_id
   ) AS i
   JOIN moz_places h ON h.id = i.place_id
   LEFT JOIN moz_openpages_temp t
          ON t.url = h.url
         AND t.userContextId = :userContextId
   WHERE AUTOCOMPLETE_MATCH(NULL, h.url,
                            IFNULL(btitle, h.title), tags,
                            h.visit_count, h.typed, bookmarked,
                            t.open_count,
                            :matchBehavior, :searchBehavior)
   ORDER BY rank DESC, h.frecency DESC
   LIMIT :maxResults`;

// Result row indexes for originQuery()
const QUERYINDEX_ORIGIN_AUTOFILLED_VALUE = 1;
const QUERYINDEX_ORIGIN_URL = 2;
const QUERYINDEX_ORIGIN_FRECENCY = 3;

// `WITH` clause for the autofill queries.  autofill_frecency_threshold.value is
// the mean of all moz_origins.frecency values + stddevMultiplier * one standard
// deviation.  This is inlined directly in the SQL (as opposed to being a custom
// Sqlite function for example) in order to be as efficient as possible.
const SQL_AUTOFILL_WITH = `
  WITH
  frecency_stats(count, sum, squares) AS (
    SELECT
      CAST((SELECT IFNULL(value, 0.0) FROM moz_meta WHERE key = "origin_frecency_count") AS REAL),
      CAST((SELECT IFNULL(value, 0.0) FROM moz_meta WHERE key = "origin_frecency_sum") AS REAL),
      CAST((SELECT IFNULL(value, 0.0) FROM moz_meta WHERE key = "origin_frecency_sum_of_squares") AS REAL)
  ),
  autofill_frecency_threshold(value) AS (
    SELECT
      CASE count
      WHEN 0 THEN 0.0
      WHEN 1 THEN sum
      ELSE (sum / count) + (:stddevMultiplier * sqrt((squares - ((sum * sum) / count)) / count))
      END
    FROM frecency_stats
  )
`;

const SQL_AUTOFILL_FRECENCY_THRESHOLD = `(
  SELECT value FROM autofill_frecency_threshold
)`;

function originQuery(conditions = "", bookmarkedFragment = "NULL") {
  return `${SQL_AUTOFILL_WITH}
          SELECT :query_type,
                 fixed_up_host || '/',
                 IFNULL(:prefix, prefix) || moz_origins.host || '/',
                 frecency,
                 bookmarked,
                 id
          FROM (
            SELECT host,
                   host AS fixed_up_host,
                   TOTAL(frecency) AS host_frecency,
                   (
                     SELECT TOTAL(foreign_count) > 0
                     FROM moz_places
                     WHERE moz_places.origin_id = moz_origins.id
                   ) AS bookmarked
            FROM moz_origins
            WHERE host BETWEEN :searchString AND :searchString || X'FFFF'
                  ${conditions}
            GROUP BY host
            HAVING host_frecency >= ${SQL_AUTOFILL_FRECENCY_THRESHOLD}
                   OR bookmarked
            UNION ALL
            SELECT host,
                   fixup_url(host) AS fixed_up_host,
                   TOTAL(frecency) AS host_frecency,
                   (
                     SELECT TOTAL(foreign_count) > 0
                     FROM moz_places
                     WHERE moz_places.origin_id = moz_origins.id
                   ) AS bookmarked
            FROM moz_origins
            WHERE host BETWEEN 'www.' || :searchString AND 'www.' || :searchString || X'FFFF'
                  ${conditions}
            GROUP BY host
            HAVING host_frecency >= ${SQL_AUTOFILL_FRECENCY_THRESHOLD}
                   OR bookmarked
          ) AS grouped_hosts
          JOIN moz_origins ON moz_origins.host = grouped_hosts.host
          ORDER BY frecency DESC, id DESC
          LIMIT 1 `;
}

const SQL_ORIGIN_QUERY = originQuery();

const SQL_ORIGIN_PREFIX_QUERY = originQuery(
  `AND prefix BETWEEN :prefix AND :prefix || X'FFFF'`
);

const SQL_ORIGIN_BOOKMARKED_QUERY = originQuery(
  `AND bookmarked`
);

const SQL_ORIGIN_PREFIX_BOOKMARKED_QUERY = originQuery(
  `AND bookmarked
   AND prefix BETWEEN :prefix AND :prefix || X'FFFF'`,
);

// Result row indexes for urlQuery()
const QUERYINDEX_URL_URL = 1;
const QUERYINDEX_URL_STRIPPED_URL = 2;
const QUERYINDEX_URL_FRECENCY = 3;

function urlQuery(conditions1, conditions2) {
  // We limit the search to places that are either bookmarked or have a frecency
  // over some small, arbitrary threshold (20) in order to avoid scanning as few
  // rows as possible.  Keep in mind that we run this query every time the user
  // types a key when the urlbar value looks like a URL with a path.
  return `/* do not warn (bug no): cannot use an index to sort */
          SELECT :query_type,
                 url,
                 :strippedURL,
                 frecency,
                 foreign_count > 0 AS bookmarked,
                 id
          FROM moz_places
          WHERE rev_host = :revHost
                AND (bookmarked OR frecency > 20)
                ${conditions1}
          UNION ALL
          SELECT :query_type,
                 url,
                 :strippedURL,
                 frecency,
                 foreign_count > 0 AS bookmarked,
                 id
          FROM moz_places
          WHERE rev_host = :revHost || 'www.'
                AND (bookmarked OR frecency > 20)
                ${conditions2}
          ORDER BY frecency DESC, id DESC
          LIMIT 1 `;
}

const SQL_URL_QUERY = urlQuery(
  `AND strip_prefix_and_userinfo(url) BETWEEN :strippedURL AND :strippedURL || X'FFFF'`,
  `AND strip_prefix_and_userinfo(url) BETWEEN 'www.' || :strippedURL AND 'www.' || :strippedURL || X'FFFF'`
);

const SQL_URL_PREFIX_QUERY = urlQuery(
  `AND url BETWEEN :prefix || :strippedURL AND :prefix || :strippedURL || X'FFFF'`,
  `AND url BETWEEN :prefix || 'www.' || :strippedURL AND :prefix || 'www.' || :strippedURL || X'FFFF'`
);

const SQL_URL_BOOKMARKED_QUERY = urlQuery(
  `AND bookmarked
   AND strip_prefix_and_userinfo(url) BETWEEN :strippedURL AND :strippedURL || X'FFFF'`,
  `AND bookmarked
   AND strip_prefix_and_userinfo(url) BETWEEN 'www.' || :strippedURL AND 'www.' || :strippedURL || X'FFFF'`
);

const SQL_URL_PREFIX_BOOKMARKED_QUERY = urlQuery(
  `AND bookmarked
   AND url BETWEEN :prefix || :strippedURL AND :prefix || :strippedURL || X'FFFF'`,
  `AND bookmarked
   AND url BETWEEN :prefix || 'www.' || :strippedURL AND :prefix || 'www.' || :strippedURL || X'FFFF'`
);

// Getters

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  AboutPagesUtils: "resource://gre/modules/AboutPagesUtils.jsm",
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  ExtensionSearchHandler: "resource://gre/modules/ExtensionSearchHandler.jsm",
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PlacesRemoteTabsAutocompleteProvider: "resource://gre/modules/PlacesRemoteTabsAutocompleteProvider.jsm",
  PlacesSearchAutocompleteProvider: "resource://gre/modules/PlacesSearchAutocompleteProvider.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  ProfileAge: "resource://gre/modules/ProfileAge.jsm",
  Sqlite: "resource://gre/modules/Sqlite.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProviderOpenTabs: "resource:///modules/UrlbarProviderOpenTabs.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(this, "syncUsernamePref",
                                      "services.sync.username");

// The special characters below can be typed into the urlbar to either restrict
// the search to visited history, bookmarked, tagged pages; or force a match on
// just the title text or url.
XPCOMUtils.defineLazyGetter(this, "TOKEN_TO_BEHAVIOR_MAP", () => new Map(
  Object.entries(UrlbarTokenizer.RESTRICT).map(
    ([type, char]) => [char, type.toLowerCase()]
  )
));

function setTimeout(callback, ms) {
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(callback, ms, timer.TYPE_ONE_SHOT);
  return timer;
}

const kProtocolsWithIcons = ["chrome:", "moz-extension:", "about:", "http:", "https:", "ftp:"];
function iconHelper(url) {
  if (typeof url == "string") {
    return kProtocolsWithIcons.some(p => url.startsWith(p)) ?
      "page-icon:" + url : PlacesUtils.favicons.defaultFavicon.spec;
  }
  if (url && url instanceof URL && kProtocolsWithIcons.includes(url.protocol)) {
    return "page-icon:" + url.href;
  }
  return PlacesUtils.favicons.defaultFavicon.spec;
}

// Preloaded Sites related

function PreloadedSite(url, title) {
  this.uri = Services.io.newURI(url);
  this.title = title;
  this._matchTitle = title.toLowerCase();
  this._hasWWW = this.uri.host.startsWith("www.");
  this._hostWithoutWWW = this._hasWWW ? this.uri.host.slice(4)
                                      : this.uri.host;
}

/**
 * Storage object for Preloaded Sites.
 *   add(url, title): adds a site to storage
 *   populate(sites) : populates the  storage with array of [url,title]
 *   sites[]: resulting array of sites (PreloadedSite objects)
 */
XPCOMUtils.defineLazyGetter(this, "PreloadedSiteStorage", () => Object.seal({
  sites: [],

  add(url, title) {
    let site = new PreloadedSite(url, title);
    this.sites.push(site);
  },

  populate(sites) {
    this.sites = [];
    for (let site of sites) {
      this.add(site[0], site[1]);
    }
  },
}));

XPCOMUtils.defineLazyGetter(this, "ProfileAgeCreatedPromise", async () => {
  let times = await ProfileAge();
  return times.created;
});

// Helper functions

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
function getUnfilteredSearchTokens(searchString) {
  return searchString.length ? searchString.split(REGEXP_SPACES) : [];
}

/**
 * Strips the prefix from a URL and returns the prefix and the remainder of the
 * URL.  "Prefix" is defined to be the scheme and colon, plus, if present, two
 * slashes.  If the given string is not actually a URL, then an empty prefix and
 * the string itself is returned.
 *
 * @param  str
 *         The possible URL to strip.
 * @return If `str` is a URL, then [prefix, remainder].  Otherwise, ["", str].
 */
function stripPrefix(str) {
  let match = REGEXP_STRIP_PREFIX.exec(str);
  if (!match) {
    return ["", str];
  }
  let prefix = match[0];
  if (prefix.length < str.length && str[prefix.length] == " ") {
    return ["", str];
  }
  return [prefix, str.substr(prefix.length)];
}

/**
 * Strip http and trailing separators from a spec.
 *
 * @param spec
 *        The text to modify.
 * @param trimSlash
 *        Whether to trim the trailing slash.
 * @return the modified spec.
 */
function stripHttpAndTrim(spec, trimSlash = true) {
  if (spec.startsWith("http://")) {
    spec = spec.slice(7);
  }
  if (spec.endsWith("?")) {
    spec = spec.slice(0, -1);
  }
  if (trimSlash && spec.endsWith("/")) {
    spec = spec.slice(0, -1);
  }
  return spec;
}

/**
 * Returns the key to be used for a match in a map for the purposes of removing
 * duplicate entries - any 2 matches that should be considered the same should
 * return the same key.  The type of the returned key depends on the type of the
 * match, so don't assume you can compare keys using ==.  Instead, use
 * ObjectUtils.deepEqual().
 *
 * @param   {object} match
 *          The match object.
 * @returns {value} Some opaque key object.  Use ObjectUtils.deepEqual() to
 *          compare keys.
 */
function makeKeyForMatch(match) {
  // For autofill entries, we need to have a key based on the comment rather
  // than the value field, because the latter may have been trimmed.
  if (match.hasOwnProperty("style") && match.style.includes("autofill")) {
    return [stripHttpAndTrim(match.comment), null];
  }

  let action = PlacesUtils.parseActionUrl(match.value);
  if (!action) {
    return [stripHttpAndTrim(match.value), null];
  }

  let key;
  switch (action.type) {
    case "searchengine":
      // We want to exclude search suggestion matches that simply echo back the
      // query string in the heuristic result.  For example, if the user types
      // "@engine test", we want to exclude a "test" suggestion match.
      key = [
        action.type,
        action.params.engineName,
        (action.params.searchSuggestion || action.params.searchQuery)
          .toLocaleLowerCase(),
      ];
      break;
    default:
      key = stripHttpAndTrim(action.params.url || match.value);
      break;
  }

  return [key, action];
}

/**
 * Returns whether the passed in string looks like a url.
 */
function looksLikeUrl(str, ignoreAlphanumericHosts = false) {
  // Single word including special chars.
  return !REGEXP_SPACES.test(str) &&
         (["/", "@", ":", "["].some(c => str.includes(c)) ||
          (ignoreAlphanumericHosts ?
            /^([\[\]A-Z0-9.:-]+[\.:]){3,}[\[\]A-Z0-9.:-]+$/i.test(str) :
            str.includes(".")));
}

/**
 * Returns whether the passed in string looks like an origin.
 */
function looksLikeOrigin(str) {
  // Single word not including path delimiters.
  return REGEXP_ORIGIN.test(str);
}

/**
 * Returns the portion of a string starting at the index where another string
 * begins.
 *
 * @param   {string} sourceStr
 *          The string to search within.
 * @param   {string} targetStr
 *          The string to search for.
 * @returns {string} The substring within sourceStr starting at targetStr, or
 *          the empty string if targetStr does not occur in sourceStr.
 */
function substringAt(sourceStr, targetStr) {
  let index = sourceStr.indexOf(targetStr);
  return index < 0 ? "" : sourceStr.substr(index);
}

/**
 * Returns the portion of a string starting at the index where another string
 * ends.
 *
 * @param   {string} sourceStr
 *          The string to search within.
 * @param   {string} targetStr
 *          The string to search for.
 * @returns {string} The substring within sourceStr where targetStr ends, or the
 *          empty string if targetStr does not occur in sourceStr.
 */
function substringAfter(sourceStr, targetStr) {
  let index = sourceStr.indexOf(targetStr);
  return index < 0 ? "" : sourceStr.substr(index + targetStr.length);
}

/**
 * Manages a single instance of an autocomplete search.
 *
 * The first three parameters all originate from the similarly named parameters
 * of nsIAutoCompleteSearch.startSearch().
 *
 * @param searchString
 *        The search string.
 * @param searchParam
 *        A space-delimited string of search parameters.  The following
 *        parameters are supported:
 *        * enable-actions: Include "actions", such as switch-to-tab and search
 *          engine aliases, in the results.
 *        * disable-private-actions: The search is taking place in a private
 *          window outside of permanent private-browsing mode.  The search
 *          should exclude privacy-sensitive results as appropriate.
 *        * private-window: The search is taking place in a private window,
 *          possibly in permanent private-browsing mode.  The search
 *          should exclude privacy-sensitive results as appropriate.
 *        * user-context-id: The userContextId of the selected tab.
 * @param autocompleteListener
 *        An nsIAutoCompleteObserver.
 * @param autocompleteSearch
 *        An nsIAutoCompleteSearch.
 * @param prohibitSearchSuggestions
 *        Whether search suggestions are allowed for this search.
 * @param [optional] previousResult
 *        The result object from the previous search. if available.
 */
function Search(searchString, searchParam, autocompleteListener,
                autocompleteSearch, prohibitSearchSuggestions, previousResult) {
  // We want to store the original string for case sensitive searches.
  this._originalSearchString = searchString;
  this._trimmedOriginalSearchString = searchString.trim();
  let [prefix, suffix] = stripPrefix(this._trimmedOriginalSearchString);
  this._searchString = Services.textToSubURI.unEscapeURIForUI("UTF-8", suffix);
  this._strippedPrefix = prefix.toLowerCase();

  this._matchBehavior = Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY;
  // Set the default behavior for this search.
  this._behavior = this._searchString ? UrlbarPrefs.get("defaultBehavior")
                                      : UrlbarPrefs.get("emptySearchDefaultBehavior");

  let params = new Set(searchParam.split(" "));
  this._enableActions = params.has("enable-actions");
  this._disablePrivateActions = params.has("disable-private-actions");
  this._inPrivateWindow = params.has("private-window");
  this._prohibitAutoFill = params.has("prohibit-autofill");

  let userContextId = searchParam.match(REGEXP_USER_CONTEXT_ID);
  this._userContextId = userContextId ?
                          parseInt(userContextId[1], 10) :
                          Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;

  let unfilteredTokens = getUnfilteredSearchTokens(this._searchString);

  // We handle any leading restriction character specially, in particular for
  // a search restriction we also handle the case where there's no space before
  // the query, like "?porcupine".
  if (unfilteredTokens.length > 1 &&
      this._trimmedOriginalSearchString.startsWith(unfilteredTokens[0]) &&
      Object.values(UrlbarTokenizer.RESTRICT).includes(unfilteredTokens[0])) {
    this._leadingRestrictionToken = unfilteredTokens[0];
  } else if (this._trimmedOriginalSearchString.startsWith(UrlbarTokenizer.RESTRICT.SEARCH)) {
    this._leadingRestrictionToken = UrlbarTokenizer.RESTRICT.SEARCH;
  }

  this._searchTokens = this.filterTokens(unfilteredTokens);

  // The heuristic token is the first filtered search token, but only when it's
  // actually the first thing in the search string.  If a prefix or restriction
  // character occurs first, then the heurstic token is null.  We use the
  // heuristic token to help determine the heuristic result.  It may be a Places
  // keyword, a search engine alias, an extension keyword, or simply a URL or
  // part of the search string the user has typed.  We won't know until we
  // create the heuristic result.
  this._heuristicToken =
    this._searchTokens[0] &&
      this._trimmedOriginalSearchString.startsWith(this._searchTokens[0]) ?
    this._searchTokens[0] :
    null;

  this._keywordSubstitute = null;

  this._prohibitSearchSuggestions = prohibitSearchSuggestions;

  this._listener = autocompleteListener;
  this._autocompleteSearch = autocompleteSearch;

  // Create a new result to add eventual matches.  Note we need a result
  // regardless having matches.
  let result = previousResult ||
               Cc["@mozilla.org/autocomplete/simple-result;1"]
                 .createInstance(Ci.nsIAutoCompleteSimpleResult);
  result.setSearchString(searchString);
  result.setListener({
    onValueRemoved(result, spec, removeFromDB) {
      if (removeFromDB) {
        PlacesUtils.history.remove(spec).catch(Cu.reportError);
      }
    },
    QueryInterface: ChromeUtils.generateQI([
      Ci.nsIAutoCompleteSimpleResultListener,
    ]),
  });
  // Will be set later, if needed.
  result.setDefaultIndex(-1);
  this._result = result;

  this._previousSearchMatchTypes = [];
  for (let i = 0; previousResult && i < previousResult.matchCount; ++i) {
    let style = previousResult.getStyleAt(i);
    if (style.includes("heuristic")) {
      this._previousSearchMatchTypes.push(UrlbarUtils.MATCH_GROUP.HEURISTIC);
    } else if (style.includes("suggestion")) {
      this._previousSearchMatchTypes.push(UrlbarUtils.MATCH_GROUP.SUGGESTION);
    } else if (style.includes("extension")) {
      this._previousSearchMatchTypes.push(UrlbarUtils.MATCH_GROUP.EXTENSION);
    } else {
      this._previousSearchMatchTypes.push(UrlbarUtils.MATCH_GROUP.GENERAL);
    }
  }

  // Used to limit the number of adaptive results.
  this._adaptiveCount = 0;
  this._extraAdaptiveRows = [];

  // Used to limit the number of remote tab results.
  this._extraRemoteTabRows = [];

  // This is a replacement for this._result.matchCount, to be used when you need
  // to check how many "current" matches have been inserted.
  // Indeed this._result.matchCount may include matches from the previous search.
  this._currentMatchCount = 0;

  // These are used to avoid adding duplicate entries to the results.
  this._usedURLs = [];
  this._usedPlaceIds = new Set();

  // Counters for the number of matches per MATCH_GROUP.
  this._counts = Object.values(UrlbarUtils.MATCH_GROUP)
                       .reduce((o, p) => { o[p] = 0; return o; }, {});
}

Search.prototype = {
  /**
   * Enables the desired AutoComplete behavior.
   *
   * @param type
   *        The behavior type to set.
   */
  setBehavior(type) {
    type = type.toUpperCase();
    this._behavior |=
      Ci.mozIPlacesAutoComplete["BEHAVIOR_" + type];
  },

  /**
   * Determines if the specified AutoComplete behavior is set.
   *
   * @param aType
   *        The behavior type to test for.
   * @return true if the behavior is set, false otherwise.
   */
  hasBehavior(type) {
    let behavior = Ci.mozIPlacesAutoComplete["BEHAVIOR_" + type.toUpperCase()];

    if (this._disablePrivateActions &&
        behavior == Ci.mozIPlacesAutoComplete.BEHAVIOR_OPENPAGE) {
      return false;
    }

    return this._behavior & behavior;
  },

  /**
   * Used to delay the most complex queries, to save IO while the user is
   * typing.
   */
  _sleepResolve: null,
  _sleep(aTimeMs) {
    // Reuse a single instance to try shaving off some usless work before
    // the first query.
    if (!this._sleepTimer)
      this._sleepTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    return new Promise(resolve => {
      this._sleepResolve = resolve;
      this._sleepTimer.initWithCallback(resolve, aTimeMs,
                                        Ci.nsITimer.TYPE_ONE_SHOT);
    });
  },

  /**
   * Given an array of tokens, this function determines which query should be
   * ran.  It also removes any special search tokens.  The given array of tokens
   * is modified in place and returned.
   *
   * @param tokens
   *        An array of search tokens.  This array is modified in place.
   * @return The given array of tokens, modified to remove special search tokens.
   */
  filterTokens(tokens) {
    let foundToken = false;
    // Set the proper behavior while filtering tokens.
    for (let i = tokens.length - 1; i >= 0; i--) {
      let behavior = TOKEN_TO_BEHAVIOR_MAP.get(tokens[i]);
      // Don't remove the token if it didn't match, or if it's an action but
      // actions are not enabled.
      if (behavior && (behavior != "openpage" || this._enableActions)) {
        // Don't use the suggest preferences if it is a token search and
        // set the restrict bit to 1 (to intersect the search results).
        if (!foundToken) {
          foundToken = true;
          // Do not take into account previous behavior (e.g.: history, bookmark)
          this._behavior = 0;
          this.setBehavior("restrict");
        }
        this.setBehavior(behavior);
        tokens.splice(i, 1);
      }
    }

    // Set the right JavaScript behavior based on our preference.  Note that the
    // preference is whether or not we should filter JavaScript, and the
    // behavior is if we should search it or not.
    if (!UrlbarPrefs.get("filter.javascript")) {
      this.setBehavior("javascript");
    }

    return tokens;
  },

  /**
   * Stop this search.
   * After invoking this method, we won't run any more searches or heuristics,
   * and no new matches may be added to the current result.
   */
  stop() {
    // Avoid multiple calls or re-entrance.
    if (!this.pending)
      return;
    if (this._notifyTimer)
      this._notifyTimer.cancel();
    this._notifyDelaysCount = 0;
    if (this._sleepTimer)
      this._sleepTimer.cancel();
    if (this._sleepResolve) {
      this._sleepResolve();
      this._sleepResolve = null;
    }
    if (this._suggestionsFetch) {
      this._suggestionsFetch.stop();
      this._suggestionsFetch = null;
    }
    if (typeof this.interrupt == "function") {
      this.interrupt();
    }
    this.pending = false;
  },

  /**
   * Whether this search is active.
   */
  pending: true,

  /**
   * Execute the search and populate results.
   * @param conn
   *        The Sqlite connection.
   */
  async execute(conn) {
    // A search might be canceled before it starts.
    if (!this.pending)
      return;

    // Used by stop() to interrupt an eventual running statement.
    this.interrupt = () => {
      // Interrupt any ongoing statement to run the search sooner.
      if (!UrlbarProvidersManager.interruptLevel) {
        conn.interrupt();
      }
    };

    TelemetryStopwatch.start(TELEMETRY_1ST_RESULT, this);
    TelemetryStopwatch.start(TELEMETRY_6_FIRST_RESULTS, this);

    // Since we call the synchronous parseSubmissionURL function later, we must
    // wait for the initialization of PlacesSearchAutocompleteProvider first.
    await PlacesSearchAutocompleteProvider.ensureInitialized();
    if (!this.pending)
      return;

    // For any given search, we run many queries/heuristics:
    // 1) by alias (as defined in SearchService)
    // 2) inline completion from search engine resultDomains
    // 3) inline completion for origins (this._originQuery) or urls (this._urlQuery)
    // 4) directly typed in url (ie, can be navigated to as-is)
    // 5) submission for the current search engine
    // 6) Places keywords
    // 7) adaptive learning (this._adaptiveQuery)
    // 8) open pages not supported by history (this._switchToTabQuery)
    // 9) query based on match behavior
    //
    // (6) only gets run if we get any filtered tokens, since if there are no
    // tokens, there is nothing to match.
    //
    // (1), (4), (5) only get run if actions are enabled. When actions are
    // enabled, the first result is always a special result (resulting from one
    // of the queries between (1) and (6) inclusive). As such, the UI is
    // expected to auto-select the first result when actions are enabled. If the
    // first result is an inline completion result, that will also be the
    // default result and therefore be autofilled (this also happens if actions
    // are not enabled).

    // Check for Preloaded Sites Expiry before Autofill
    await this._checkPreloadedSitesExpiry();

    // If the query is simply "@", then the results should be a list of all the
    // search engines with "@" aliases, without a hueristic result.
    if (this._trimmedOriginalSearchString == "@") {
      let added = await this._addSearchEngineTokenAliasMatches();
      if (added) {
        this._cleanUpNonCurrentMatches(null);
        this._autocompleteSearch.finishSearch(true);
        return;
      }
    }

    // Add the first heuristic result, if any.  Set _addingHeuristicFirstMatch
    // to true so that when the result is added, "heuristic" can be included in
    // its style.
    this._addingHeuristicFirstMatch = true;
    let hasHeuristic = await this._matchFirstHeuristicResult(conn);
    this._addingHeuristicFirstMatch = false;
    this._cleanUpNonCurrentMatches(UrlbarUtils.MATCH_GROUP.HEURISTIC);
    if (!this.pending)
      return;

    // We sleep a little between adding the heuristicFirstMatch and matching
    // any other searches so we aren't kicking off potentially expensive
    // searches on every keystroke.
    // Though, if there's no heuristic result, we start searching immediately,
    // since autocomplete may be waiting for us.
    if (hasHeuristic) {
      await this._sleep(UrlbarPrefs.get("delay"));
      if (!this.pending)
        return;

      // If the heuristic result is a search engine result with an empty query
      // and we have either a token alias or the search restriction char, then
      // we're done.  We want to show only that single result as a clear hint
      // that the user can continue typing to search.
      // For the restriction character case, also consider a single char query
      // or just the char itself, anyway we don't return search suggestions
      // unless at least 2 chars have been typed. Thus "?__" and "? a" should
      // finish here, while "?aa" should continue.
      let emptyQueryTokenAlias = this._searchEngineAliasMatch &&
                                  this._searchEngineAliasMatch.isTokenAlias &&
                                  !this._searchEngineAliasMatch.query;
      let emptySearchRestriction = this._trimmedOriginalSearchString.length <= 3 &&
                                   this._leadingRestrictionToken == UrlbarTokenizer.RESTRICT.SEARCH &&
                                   /\s*\S?$/.test(this._trimmedOriginalSearchString);
      if (emptySearchRestriction || emptyQueryTokenAlias) {
        this._cleanUpNonCurrentMatches(null, false);
        this._autocompleteSearch.finishSearch(true);
        return;
      }
    }

    // Only add extension suggestions if the first token is a registered keyword
    // and the search string has characters after the first token.
    let extensionsCompletePromise = Promise.resolve();
    if (this._heuristicToken &&
        ExtensionSearchHandler.isKeywordRegistered(this._heuristicToken) &&
        substringAfter(this._originalSearchString, this._heuristicToken) &&
        !this._searchEngineAliasMatch) {
      // Do not await on this, since extensions cannot notify when they are done
      // adding results, it may take too long.
      extensionsCompletePromise = this._matchExtensionSuggestions();
    } else if (ExtensionSearchHandler.hasActiveInputSession()) {
      ExtensionSearchHandler.handleInputCancelled();
    }

    // Start adding search suggestions, unless they aren't required or the
    // window is private.
    let searchSuggestionsCompletePromise = Promise.resolve();
    if (this._enableActions &&
        this.hasBehavior("search") &&
        !this._inPrivateWindow) {
      let query =
        this._searchEngineAliasMatch ? this._searchEngineAliasMatch.query :
        substringAt(this._originalSearchString, this._searchTokens[0]);
      if (query) {
        // Limit the string sent for search suggestions to a maximum length.
        query = query.substr(0, UrlbarPrefs.get("maxCharsForSearchSuggestions"));
        // Don't add suggestions if the query may expose sensitive information.
        if (!this._prohibitSearchSuggestionsFor(query)) {
          let engine;
          if (this._searchEngineAliasMatch) {
            engine = this._searchEngineAliasMatch.engine;
          } else {
            engine = await PlacesSearchAutocompleteProvider.currentEngine();
            if (!this.pending) {
              return;
            }
          }
          let alias =
            this._searchEngineAliasMatch &&
            this._searchEngineAliasMatch.alias ||
            "";
          searchSuggestionsCompletePromise =
            this._matchSearchSuggestions(engine, query, alias);
          // If the user has used a search engine token alias, then the only
          // results we want to show are suggestions from that engine, so we're
          // done.  We're also done if we're restricting results to suggestions.
          if ((this._searchEngineAliasMatch &&
               this._searchEngineAliasMatch.isTokenAlias) ||
              this.hasBehavior("restrict")) {
            // Wait for the suggestions to be added.
            await searchSuggestionsCompletePromise;
            this._cleanUpNonCurrentMatches(null);
            this._autocompleteSearch.finishSearch(true);
            return;
          }
        }
      }
    }
    // In any case, clear previous suggestions.
    searchSuggestionsCompletePromise.then(() => {
      this._cleanUpNonCurrentMatches(UrlbarUtils.MATCH_GROUP.SUGGESTION);
    });

    // Run the adaptive query first.
    await conn.executeCached(this._adaptiveQuery[0], this._adaptiveQuery[1],
                             this._onResultRow.bind(this));
    if (!this.pending)
      return;

    // Then fetch remote tabs.
    if (this._enableActions && this.hasBehavior("openpage")) {
      await this._matchRemoteTabs();
      if (!this.pending)
        return;
    }

    // Get the final query, based on the tokens found in the search string and
    // the keyword substitution, if any.
    let queries = [];
    // "openpage" behavior is supported by the default query.
    // _switchToTabQuery instead returns only pages not supported by history.
    if (this.hasBehavior("openpage")) {
      queries.push(this._switchToTabQuery);
    }
    queries.push(this._searchQuery);

    // Finally run all the other queries.
    for (let [query, params] of queries) {
      await conn.executeCached(query, params, this._onResultRow.bind(this));
      if (!this.pending)
        return;
    }

    // If we have some unused adaptive matches, add them now.
    while (this._extraAdaptiveRows.length &&
           this._currentMatchCount < UrlbarPrefs.get("maxRichResults")) {
      this._addFilteredQueryMatch(this._extraAdaptiveRows.shift());
    }

    // If we have some unused remote tab matches, add them now.
    while (this._extraRemoteTabRows.length &&
          this._currentMatchCount < UrlbarPrefs.get("maxRichResults")) {
      this._addMatch(this._extraRemoteTabRows.shift());
    }

    // Ideally we should wait until MATCH_BOUNDARY_ANYWHERE, but that query
    // may be really slow and we may end up showing old results for too long.
    this._cleanUpNonCurrentMatches(UrlbarUtils.MATCH_GROUP.GENERAL);

    this._matchAboutPages();

    // If we do not have enough matches search again with MATCH_ANYWHERE, to
    // get more matches.
    let count = this._counts[UrlbarUtils.MATCH_GROUP.GENERAL] +
                this._counts[UrlbarUtils.MATCH_GROUP.HEURISTIC];
    if (count < UrlbarPrefs.get("maxRichResults")) {
      this._matchBehavior = Ci.mozIPlacesAutoComplete.MATCH_ANYWHERE;
      for (let [query, params] of [ this._adaptiveQuery,
                                    this._searchQuery ]) {
        await conn.executeCached(query, params, this._onResultRow.bind(this));
        if (!this.pending)
          return;
      }
    }

    this._matchPreloadedSites();

    // Ensure to fill any remaining space.
    await searchSuggestionsCompletePromise;
    await extensionsCompletePromise;
  },

  _shouldMatchAboutPages() {
    // Only autocomplete input that starts with 'about:' and has at least 1 more
    // character.
    return (this._strippedPrefix == "about:" &&
            this._searchString);
  },

  _matchAboutPages() {
    if (!this._shouldMatchAboutPages()) {
      return;
    }
    for (const url of AboutPagesUtils.visibleAboutUrls) {
      if (url.startsWith(`about:${this._searchString}`)) {
        this._addMatch({
          value: url,
          comment: url,
          frecency: FRECENCY_DEFAULT,
        });
      }
    }
  },

  _matchAboutPageForAutofill() {
    if (!this._shouldMatchAboutPages()) {
      return false;
    }
    for (const url of AboutPagesUtils.visibleAboutUrls) {
      if (url.startsWith(`about:${this._searchString.toLowerCase()}`)) {
        this._result.setDefaultIndex(0);
        this._addAutofillMatch(
          url.substr(6),
          url,
        );
        return true;
      }
    }
    return false;
  },

  async _checkPreloadedSitesExpiry() {
    if (!UrlbarPrefs.get("usepreloadedtopurls.enabled"))
      return;
    let profileCreationDate = await ProfileAgeCreatedPromise;
    let daysSinceProfileCreation = (Date.now() - profileCreationDate) / MS_PER_DAY;
    if (daysSinceProfileCreation > UrlbarPrefs.get("usepreloadedtopurls.expire_days"))
      Services.prefs.setBoolPref("browser.urlbar.usepreloadedtopurls.enabled", false);
  },

  _matchPreloadedSites() {
    if (!UrlbarPrefs.get("usepreloadedtopurls.enabled")) {
      return;
    }

    if (!this._searchString) {
      // The user hasn't typed anything, or they've only typed a scheme.
      return;
    }

    for (let site of PreloadedSiteStorage.sites) {
      let url = site.uri.spec;
      if ((!this._strippedPrefix || url.startsWith(this._strippedPrefix)) &&
          (site.uri.host.includes(this._searchString) ||
           site._matchTitle.includes(this._searchString))) {
        this._addMatch({
          value: url,
          comment: site.title,
          style: "preloaded-top-site",
          frecency: FRECENCY_DEFAULT - 1,
        });
      }
    }
  },

  _matchPreloadedSiteForAutofill() {
    if (!UrlbarPrefs.get("usepreloadedtopurls.enabled")) {
      return false;
    }

    let matchedSite = PreloadedSiteStorage.sites.find(site => {
      return (!this._strippedPrefix ||
              site.uri.spec.startsWith(this._strippedPrefix)) &&
             (site.uri.host.startsWith(this._searchString) ||
              site.uri.host.startsWith("www." + this._searchString));
    });
    if (!matchedSite) {
      return false;
    }

    this._result.setDefaultIndex(0);

    let url = matchedSite.uri.spec;
    let value = stripPrefix(url)[1];
    value = value.substr(value.indexOf(this._searchString));

    this._addAutofillMatch(
      value,
      url,
      Infinity,
      ["preloaded-top-site"]
    );
    return true;
  },

  /**
   * Adds matches for all the engines with "@" aliases, if any.
   *
   * @returns {bool} True if any results were added, false if not.
   */
  async _addSearchEngineTokenAliasMatches() {
    let engines = await PlacesSearchAutocompleteProvider.tokenAliasEngines();
    if (!engines || !engines.length) {
      return false;
    }
    for (let { engine, tokenAliases } of engines) {
      this._addSearchEngineMatch({
        engine,
        alias: tokenAliases[0],
      });
    }
    return true;
  },

  async _matchSearchEngineTokenAliasForAutofill() {
    // We need an "@engine" heuristic token.
    let token = this._heuristicToken;
    if (!token || token.length == 1 || !token.startsWith("@")) {
      return false;
    }

    // See if any engine has a token alias that starts with the heuristic token.
    let engines = await PlacesSearchAutocompleteProvider.tokenAliasEngines();
    for (let { engine, tokenAliases } of engines) {
      for (let alias of tokenAliases) {
        if (alias.startsWith(token.toLocaleLowerCase())) {
          // We found one.  The match we add here is a little special compared
          // to others.  It needs to be an autofill match and its `value` must
          // be the string that will be autofilled so that the controller will
          // autofill it.  But it also must be a searchengine action so that the
          // front end will style it as a search engine result.  The front end
          // uses `finalCompleteValue` as the URL for autofill results, so set
          // that to the moz-action URL.
          let aliasPreservingUserCase = token + alias.substr(token.length);
          let value = aliasPreservingUserCase + " ";
          this._addMatch({
            value,
            finalCompleteValue: PlacesUtils.mozActionURI("searchengine", {
              engineName: engine.name,
              alias: aliasPreservingUserCase,
              input: value,
              searchQuery: "",
            }),
            comment: engine.name,
            frecency: FRECENCY_DEFAULT,
            style: "autofill action searchengine",
            icon: engine.iconURI ? engine.iconURI.spec : null,
          });
          this._result.setDefaultIndex(0);

          // Set _searchEngineAliasMatch with an empty query so that we don't
          // attempt to add any more matches.  When a token alias is autofilled,
          // the only match should be the one we just added.
          this._searchEngineAliasMatch = {
            engine,
            alias: aliasPreservingUserCase,
            query: "",
            isTokenAlias: true,
          };

          return true;
        }
      }
    }

    return false;
  },

  async _matchFirstHeuristicResult(conn) {
    // We always try to make the first result a special "heuristic" result.  The
    // heuristics below determine what type of result it will be, if any.

    let hasSearchTerms = this._searchTokens.length > 0;

    if (hasSearchTerms) {
      // It may be a keyword registered by an extension.
      let matched = await this._matchExtensionHeuristicResult();
      if (matched) {
        return true;
      }
    }

    if (this._enableActions && hasSearchTerms) {
      // It may be a search engine with an alias - which works like a keyword.
      let matched = await this._matchSearchEngineAlias();
      if (matched) {
        return true;
      }
    }

    if (this.pending && hasSearchTerms) {
      // It may be a Places keyword.
      let matched = await this._matchPlacesKeyword();
      if (matched) {
        return true;
      }
    }

    let shouldAutofill = this._shouldAutofill;

    if (this.pending && shouldAutofill) {
      // It may also look like an about: link.
      let matched = await this._matchAboutPageForAutofill();
      if (matched) {
        return true;
      }
    }

    if (this.pending && shouldAutofill) {
      // It may also look like a URL we know from the database.
      let matched = await this._matchKnownUrl(conn);
      if (matched) {
        return true;
      }
    }

    if (this.pending && shouldAutofill) {
      // Or it may look like a search engine domain.
      let matched = await this._matchSearchEngineDomain();
      if (matched) {
        return true;
      }
    }

    if (this.pending && shouldAutofill) {
      let matched = this._matchPreloadedSiteForAutofill();
      if (matched) {
        return true;
      }
    }

    if (this.pending && shouldAutofill) {
      let matched = await this._matchSearchEngineTokenAliasForAutofill();
      if (matched) {
        return true;
      }
    }

    if (this.pending && hasSearchTerms && this._enableActions) {
      // If we don't have a result that matches what we know about, then
      // we use a fallback for things we don't know about.

      // We may not have auto-filled, but this may still look like a URL.
      // However, even if the input is a valid URL, we may not want to use
      // it as such. This can happen if the host would require whitelisting,
      // but isn't in the whitelist.
      let matched = await this._matchUnknownUrl();
      if (matched) {
        // Since we can't tell if this is a real URL and
        // whether the user wants to visit or search for it,
        // we always provide an alternative searchengine match.
        try {
          new URL(this._originalSearchString);
        } catch (ex) {
          if (UrlbarPrefs.get("keyword.enabled") &&
              !looksLikeUrl(this._originalSearchString, true)) {
            this._addingHeuristicFirstMatch = false;
            await this._matchCurrentSearchEngine();
            this._addingHeuristicFirstMatch = true;
          }
        }
        return true;
      }
    }

    if (this.pending && this._enableActions && this._originalSearchString) {
      // When all else fails, and the search string is non-empty, we search
      // using the current search engine.
      let matched = await this._matchCurrentSearchEngine();
      if (matched) {
        return true;
      }
    }

    return false;
  },

  _matchSearchSuggestions(engine, searchString, alias) {
    this._suggestionsFetch =
      PlacesSearchAutocompleteProvider.newSuggestionsFetch(
        engine,
        searchString,
        this._inPrivateWindow,
        UrlbarPrefs.get("maxHistoricalSearchSuggestions"),
        UrlbarPrefs.get("maxRichResults") - UrlbarPrefs.get("maxHistoricalSearchSuggestions"),
        this._userContextId
      );
    return this._suggestionsFetch.fetchCompletePromise.then(() => {
      // The fetch has been canceled already.
      if (!this._suggestionsFetch) {
        return;
      }
      if (this._suggestionsFetch.resultsCount >= 0 &&
          this._suggestionsFetch.resultsCount < 2) {
        // The original string is used to properly compare with the next fetch.
        this._lastLowResultsSearchSuggestion = this._originalSearchString;
      }
      while (this.pending) {
        let result = this._suggestionsFetch.consume();
        if (!result)
          break;
        let { suggestion, historical } = result;
        if (!looksLikeUrl(suggestion)) {
          this._addSearchEngineMatch({
            engine,
            alias,
            query: searchString,
            suggestion,
            historical,
          });
        }
      }
    }).catch(Cu.reportError);
  },

  _prohibitSearchSuggestionsFor(searchString) {
    if (this._prohibitSearchSuggestions)
      return true;

    // Never prohibit suggestions when the user has used a search engine token
    // alias.  We want "@engine query" to return suggestions from the engine.
    if (this._searchEngineAliasMatch &&
        this._searchEngineAliasMatch.isTokenAlias) {
      return false;
    }

    // Suggestions for a single letter are unlikely to be useful.
    if (searchString.length < 2)
      return true;

    // The first token may be a whitelisted host.
    if (this._searchTokens.length == 1 &&
        REGEXP_SINGLEWORD_HOST.test(this._searchTokens[0]) &&
        Services.uriFixup.isDomainWhitelisted(this._searchTokens[0], -1)) {
      return true;
    }

    // Disallow fetching search suggestions for strings that start off looking
    // like urls.
    if (DISALLOWED_URLLIKE_PREFIXES.some(prefix => this._trimmedOriginalSearchString == prefix) ||
        DISALLOWED_URLLIKE_PREFIXES.some(prefix => this._trimmedOriginalSearchString.startsWith(prefix + ":"))) {
      return true;
    }

    // Disallow fetching search suggestions for strings looking like URLs, to
    // avoid disclosing information about networks or passwords.
    return this._searchTokens.some(looksLikeUrl);
  },

  async _matchKnownUrl(conn) {
    let gotResult = false;

    // If search string looks like an origin, try to autofill against origins.
    // Otherwise treat it as a possible URL.  When the string has only one slash
    // at the end, we still treat it as an URL.
    let query, params;
    if (looksLikeOrigin(this._searchString)) {
      [query, params] = this._originQuery;
    } else {
      [query, params] = this._urlQuery;
    }

    // _urlQuery doesn't always return a query.
    if (query) {
      await conn.executeCached(query, params, (row, cancel) => {
        gotResult = true;
        this._onResultRow(row, cancel);
      });
    }
    return gotResult;
  },

  _matchExtensionHeuristicResult() {
    if (this._heuristicToken &&
        ExtensionSearchHandler.isKeywordRegistered(this._heuristicToken) &&
        substringAfter(this._originalSearchString, this._heuristicToken)) {
      let description =
        ExtensionSearchHandler.getDescription(this._heuristicToken);
      this._addExtensionMatch(this._originalSearchString, description);
      return true;
    }
    return false;
  },

  async _matchPlacesKeyword() {
    if (!this._heuristicToken) {
      return false;
    }
    let keyword = this._heuristicToken;
    let entry = await PlacesUtils.keywords.fetch(keyword);
    if (!entry) {
      return false;
    }

    let searchString =
      substringAfter(this._originalSearchString, keyword).trim();

    let url = null, postData = null;
    try {
      [url, postData] =
        await BrowserUtils.parseUrlAndPostData(entry.url.href,
                                               entry.postData,
                                               searchString);
    } catch (ex) {
      // It's not possible to bind a param to this keyword.
      return false;
    }

    let style = "keyword";
    let value = url;
    if (this._enableActions) {
      style = "action " + style;
      value = PlacesUtils.mozActionURI("keyword", {
        url,
        input: this._originalSearchString,
        postData,
      });
    }
    // The title will end up being "host: queryString"
    let comment = entry.url.host;

    this._addMatch({
      value,
      comment,
      // Don't use the url with replaced strings, since the icon doesn't change
      // but the string does, it may cause pointless icon flicker on typing.
      icon: iconHelper(entry.url),
      style,
      frecency: Infinity,
    });
    if (!this._keywordSubstitute) {
      this._keywordSubstitute = entry.url.host;
    }
    return true;
  },

  async _matchSearchEngineDomain() {
    if (!UrlbarPrefs.get("autoFill.searchEngines")) {
      return false;
    }
    if (!this._searchString) {
      return false;
    }

    // PlacesSearchAutocompleteProvider only matches against engine domains.
    // Remove an eventual trailing slash from the search string (without the
    // prefix) and check if the resulting string is worth matching.
    // Later, we'll verify that the found result matches the original
    // searchString and eventually discard it.
    let searchStr = this._searchString;
    if (searchStr.indexOf("/") == searchStr.length - 1) {
      searchStr = searchStr.slice(0, -1);
    }
    // If the search string looks more like a url than a domain, bail out.
    if (!looksLikeOrigin(searchStr)) {
      return false;
    }

    let engine =
      await PlacesSearchAutocompleteProvider.engineForDomainPrefix(searchStr);
    if (!engine) {
      return false;
    }
    let url = engine.searchForm;
    let domain = engine.getResultDomain();
    // Verify that the match we got is acceptable. Autofilling "example/" to
    // "example.com/" would not be good.
    if ((this._strippedPrefix && !url.startsWith(this._strippedPrefix)) ||
        !(domain + "/").includes(this._searchString)) {
      return false;
    }

    // The value that's autofilled in the input is the prefix the user typed, if
    // any, plus the portion of the engine domain that the user typed.  Append a
    // trailing slash too, as is usual with autofill.
    let value =
      this._strippedPrefix + domain.substr(domain.indexOf(searchStr)) + "/";

    let finalCompleteValue = url;
    try {
      let fixupInfo = Services.uriFixup.getFixupURIInfo(url, 0);
      if (fixupInfo.fixedURI) {
        finalCompleteValue = fixupInfo.fixedURI.spec;
      }
    } catch (ex) {}

    this._result.setDefaultIndex(0);
    this._addMatch({
      value,
      finalCompleteValue,
      comment: engine.name,
      icon: engine.iconURI ? engine.iconURI.spec : null,
      style: "priority-search",
      frecency: Infinity,
    });
    return true;
  },

  async _matchSearchEngineAlias() {
    if (!this._heuristicToken) {
      return false;
    }

    let alias = this._heuristicToken;
    let engine = await PlacesSearchAutocompleteProvider.engineForAlias(alias);
    if (!engine) {
      return false;
    }

    this._searchEngineAliasMatch = {
      engine,
      alias,
      query: substringAfter(this._originalSearchString, alias).trim(),
      isTokenAlias: alias.startsWith("@"),
    };
    this._addSearchEngineMatch(this._searchEngineAliasMatch);
    if (!this._keywordSubstitute) {
      this._keywordSubstitute = engine.getResultDomain();
    }
    return true;
  },

  async _matchCurrentSearchEngine() {
    let engine = await PlacesSearchAutocompleteProvider.currentEngine();
    if (!engine || !this.pending) {
      return false;
    }
    // Strip a leading restriction char.
    let query = this._leadingRestrictionToken ?
      substringAfter(this._trimmedOriginalSearchString, this._leadingRestrictionToken).trim() :
      this._trimmedOriginalSearchString;
    this._addSearchEngineMatch({ engine, query });
    return true;
  },

  _addExtensionMatch(content, comment) {
    let count = this._counts[UrlbarUtils.MATCH_GROUP.EXTENSION] +
                this._counts[UrlbarUtils.MATCH_GROUP.HEURISTIC];
    if (count >= UrlbarUtils.MAXIMUM_ALLOWED_EXTENSION_MATCHES) {
      return;
    }

    this._addMatch({
      value: PlacesUtils.mozActionURI("extension", {
        content,
        keyword: this._heuristicToken,
      }),
      comment,
      icon: "chrome://browser/content/extension.svg",
      style: "action extension",
      frecency: Infinity,
      type: UrlbarUtils.MATCH_GROUP.EXTENSION,
    });
  },

  /**
   * Adds a search engine match.
   *
   * @param {nsISearchEngine} engine
   *        The search engine associated with the match.
   * @param {string} [query]
   *        The search query string.
   * @param {string} [alias]
   *        The search engine alias associated with the match, if any.
   * @param {string} [suggestion]
   *        The suggestion from the search engine, if you're adding a suggestion
   *        match.
   * @param {bool} [historical]
   *        True if you're adding a suggestion match and the suggestion is from
   *        the user's local history (and not the search engine).
   */
  _addSearchEngineMatch({engine,
                         query = "",
                         alias = undefined,
                         suggestion = undefined,
                         historical = false}) {
    let actionURLParams = {
      engineName: engine.name,
      searchQuery: query,
    };

    if (suggestion) {
      // `input` should include the alias.
      actionURLParams.input = (alias ? `${alias} ` : "") + suggestion;
    } else if (alias && !query) {
      // `input` should have a trailing space so that when the user selects the
      // result, they can start typing their query without first having to enter
      // a space between the alias and query.
      actionURLParams.input = `${alias} `;
    } else {
      actionURLParams.input = this._originalSearchString;
    }

    let match = {
      comment: engine.name,
      icon: engine.iconURI && !suggestion ? engine.iconURI.spec : null,
      style: "action searchengine",
      frecency: FRECENCY_DEFAULT,
    };

    if (alias) {
      actionURLParams.alias = alias;
      match.style += " alias";
    }
    if (suggestion) {
      actionURLParams.searchSuggestion = suggestion;
      match.style += " suggestion";
      match.type = UrlbarUtils.MATCH_GROUP.SUGGESTION;
    }

    match.value = PlacesUtils.mozActionURI("searchengine", actionURLParams);
    this._addMatch(match);
  },

  _matchExtensionSuggestions() {
    let promise = ExtensionSearchHandler.handleSearch(this._heuristicToken, this._originalSearchString,
      suggestions => {
        for (let suggestion of suggestions) {
          let content = `${this._heuristicToken} ${suggestion.content}`;
          this._addExtensionMatch(content, suggestion.description);
        }
      }
    );
    // Remove previous search matches sooner than the maximum timeout, otherwise
    // matches may appear stale for a long time.
    // This is necessary because WebExtensions don't have a method to notify
    // that they are done providing results, so they could be pending forever.
    setTimeout(() => this._cleanUpNonCurrentMatches(UrlbarUtils.MATCH_GROUP.EXTENSION), 100);

    // Since the extension has no way to signale when it's done pushing
    // results, we add a timeout racing with the addition.
    let timeoutPromise = new Promise(resolve => {
      setTimeout(resolve, MAXIMUM_ALLOWED_EXTENSION_TIME_MS);
    });
    return Promise.race([timeoutPromise, promise]).catch(Cu.reportError);
  },

  async _matchRemoteTabs() {
    // Bail out early for non-sync users.
    if (!syncUsernamePref) {
      return;
    }
    let matches = await PlacesRemoteTabsAutocompleteProvider.getMatches(this._originalSearchString);
    for (let {url, title, icon, deviceName, lastUsed} of matches) {
      // It's rare that Sync supplies the icon for the page (but if it does, it
      // is a string URL)
      if (!icon) {
        icon = iconHelper(url);
      } else {
        icon = PlacesUtils.favicons
                          .getFaviconLinkForIcon(Services.io.newURI(icon)).spec;
      }

      let match = {
        // We include the deviceName in the action URL so we can render it in
        // the URLBar.
        value: PlacesUtils.mozActionURI("remotetab", { url, deviceName }),
        comment: title || url,
        style: "action remotetab",
        // we want frecency > FRECENCY_DEFAULT so it doesn't get pushed out
        // by "remote" matches.
        frecency: FRECENCY_DEFAULT + 1,
        icon,
      };
      if (lastUsed > (Date.now() - RECENT_REMOTE_TAB_THRESHOLD_MS)) {
        this._addMatch(match);
      } else {
        this._extraRemoteTabRows.push(match);
      }
    }
  },

  // TODO (bug 1054814): Use visited URLs to inform which scheme to use, if the
  // scheme isn't specificed.
  _matchUnknownUrl() {
    let flags = Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
                Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
    let fixupInfo = null;
    let searchUrl = this._trimmedOriginalSearchString;
    try {
      fixupInfo = Services.uriFixup.getFixupURIInfo(searchUrl,
                                                    flags);
    } catch (e) {
      if (e.result == Cr.NS_ERROR_MALFORMED_URI &&
          !UrlbarPrefs.get("keyword.enabled")) {
        let value = PlacesUtils.mozActionURI("visiturl", {
          url: searchUrl,
          input: searchUrl,
        });
        this._addMatch({
          value,
          comment: searchUrl,
          style: "action visiturl",
          frecency: Infinity,
        });

        return true;
      }
      return false;
    }

    // If the URI cannot be fixed or the preferred URI would do a keyword search,
    // that basically means this isn't useful to us. Note that
    // fixupInfo.keywordAsSent will never be true if the keyword.enabled pref
    // is false or there are no engines, so in that case we will always return
    // a "visit".
    if (!fixupInfo.fixedURI || fixupInfo.keywordAsSent)
      return false;

    let uri = fixupInfo.fixedURI;
    // Check the host, as "http:///" is a valid nsIURI, but not useful to us.
    // But, some schemes are expected to have no host. So we check just against
    // schemes we know should have a host. This allows new schemes to be
    // implemented without us accidentally blocking access to them.
    let hostExpected = ["http", "https", "ftp", "chrome"].includes(uri.scheme);
    if (hostExpected && !uri.host)
      return false;

    // getFixupURIInfo() escaped the URI, so it may not be pretty.  Embed the
    // escaped URL in the action URI since that URL should be "canonical".  But
    // pass the pretty, unescaped URL as the match comment, since it's likely
    // to be displayed to the user, and in any case the front-end should not
    // rely on it being canonical.
    let escapedURL = uri.displaySpec;
    let displayURL = Services.textToSubURI.unEscapeURIForUI("UTF-8", escapedURL);

    let value = PlacesUtils.mozActionURI("visiturl", {
      url: escapedURL,
      input: searchUrl,
    });

    let match = {
      value,
      comment: displayURL,
      style: "action visiturl",
      frecency: Infinity,
    };

    // We don't know if this url is in Places or not, and checking that would
    // be expensive. Thus we also don't know if we may have an icon.
    // If we'd just try to fetch the icon for the typed string, we'd cause icon
    // flicker, since the url keeps changing while the user types.
    // By default we won't provide an icon, but for the subset of urls with a
    // host we'll check for a typed slash and set favicon for the host part.
    if (hostExpected &&
        (searchUrl.endsWith("/") || uri.pathQueryRef.length > 1)) {
      match.icon = `page-icon:${uri.prePath}/`;
    }

    this._addMatch(match);
    return true;
  },

  _onResultRow(row, cancel) {
    let queryType = row.getResultByIndex(QUERYINDEX_QUERYTYPE);
    switch (queryType) {
      case QUERYTYPE_AUTOFILL_ORIGIN:
        this._result.setDefaultIndex(0);
        this._addOriginAutofillMatch(row);
        break;
      case QUERYTYPE_AUTOFILL_URL:
        this._result.setDefaultIndex(0);
        this._addURLAutofillMatch(row);
        break;
      case QUERYTYPE_ADAPTIVE:
        this._addAdaptiveQueryMatch(row);
        break;
      case QUERYTYPE_FILTERED:
        this._addFilteredQueryMatch(row);
        break;
    }
    // If the search has been canceled by the user or by _addMatch, or we
    // fetched enough results, we can stop the underlying Sqlite query.
    let count = this._counts[UrlbarUtils.MATCH_GROUP.GENERAL] +
                this._counts[UrlbarUtils.MATCH_GROUP.HEURISTIC];
    if (!this.pending || count >= UrlbarPrefs.get("maxRichResults")) {
      cancel();
    }
  },

  _maybeRestyleSearchMatch(match) {
    // Return if the URL does not represent a search result.
    let parseResult =
      PlacesSearchAutocompleteProvider.parseSubmissionURL(match.value);
    if (!parseResult) {
      return;
    }

    // Do not apply the special style if the user is doing a search from the
    // location bar but the entered terms match an irrelevant portion of the
    // URL. For example, "https://www.google.com/search?q=terms&client=firefox"
    // when searching for "Firefox".
    let terms = parseResult.terms.toLowerCase();
    if (this._searchTokens.length > 0 &&
        this._searchTokens.every(token => !terms.includes(token))) {
      return;
    }

    // Turn the match into a searchengine action with a favicon.
    match.value = PlacesUtils.mozActionURI("searchengine", {
      engineName: parseResult.engineName,
      input: parseResult.terms,
      searchQuery: parseResult.terms,
    });
    match.comment = parseResult.engineName;
    match.icon = match.icon || match.iconUrl;
    match.style = "action searchengine favicon";
  },

  _addMatch(match) {
    if (typeof match.frecency != "number")
      throw new Error("Frecency not provided");

    if (this._addingHeuristicFirstMatch)
      match.type = UrlbarUtils.MATCH_GROUP.HEURISTIC;
    else if (typeof match.type != "string")
      match.type = UrlbarUtils.MATCH_GROUP.GENERAL;

    // A search could be canceled between a query start and its completion,
    // in such a case ensure we won't notify any result for it.
    if (!this.pending)
      return;

    match.style = match.style || "favicon";

    // Restyle past searches, unless they are bookmarks or special results.
    if (UrlbarPrefs.get("restyleSearches") && match.style == "favicon") {
      this._maybeRestyleSearchMatch(match);
    }

    if (this._addingHeuristicFirstMatch) {
      match.style += " heuristic";
    }

    match.icon = match.icon || "";
    match.finalCompleteValue = match.finalCompleteValue || "";

    let {index, replace} = this._getInsertIndexForMatch(match);
    if (index == -1)
      return;
    if (replace) { // Replacing an existing match from the previous search.
      this._result.removeMatchAt(index);
    }
    this._result.insertMatchAt(index,
                               match.value,
                               match.comment,
                               match.icon,
                               match.style,
                               match.finalCompleteValue);
    this._currentMatchCount++;
    this._counts[match.type]++;

    if (this._currentMatchCount == 1)
      TelemetryStopwatch.finish(TELEMETRY_1ST_RESULT, this);
    if (this._currentMatchCount == 6)
      TelemetryStopwatch.finish(TELEMETRY_6_FIRST_RESULTS, this);
    this.notifyResult(true, match.type == UrlbarUtils.MATCH_GROUP.HEURISTIC);
  },

  _getInsertIndexForMatch(match) {
    // Check for duplicates and either discard (by returning -1) the duplicate
    // or suggest to replace the original match, in case the new one is more
    // specific (for example a Remote Tab wins over History, and a Switch to Tab
    // wins over a Remote Tab).
    // Must check both id and url, cause keywords dynamically modify the url.
    // Note: this partially fixes Bug 1222435,  but not if the urls differ more
    // than just by "http://". We should still evaluate www and other schemes
    // equivalences.
    let [urlMapKey, action] = makeKeyForMatch(match);
    if ((match.placeId && this._usedPlaceIds.has(match.placeId)) ||
        this._usedURLs.some(e => ObjectUtils.deepEqual(e.key, urlMapKey))) {
      let isDupe = true;
      if (action && ["switchtab", "remotetab"].includes(action.type)) {
        // The new entry is a switch/remote tab entry, look for the duplicate
        // among current matches.
        for (let i = 0; i < this._usedURLs.length; ++i) {
          let {key: matchKey, action: matchAction, type: matchType} = this._usedURLs[i];
          if (ObjectUtils.deepEqual(matchKey, urlMapKey)) {
            isDupe = true;
            // Don't replace the match if the existing one is heuristic and the
            // new one is a switchtab, instead also add the switchtab match.
            if (matchType == UrlbarUtils.MATCH_GROUP.HEURISTIC &&
                action.type == "switchtab") {
              isDupe = false;
              // Since we allow to insert a dupe in this case, we must continue
              // checking the next matches to be sure we won't insert more than
              // one dupe. For this same reason we must reset isDupe = true for
              // each found dupe.
              continue;
            }
            if (!matchAction || action.type == "switchtab") {
              this._usedURLs[i] = {key: urlMapKey, action, type: match.type};
              return { index:  i, replace: true };
            }
            break; // Found the duplicate, no reason to continue.
          }
        }
      }
      if (isDupe) {
        return { index: -1, replace: false };
      }
    }

    // Add this to our internal tracker to ensure duplicates do not end up in
    // the result.
    // Not all entries have a place id, thus we fallback to the url for them.
    // We cannot use only the url since keywords entries are modified to
    // include the search string, and would be returned multiple times.  Ids
    // are faster too.
    if (match.placeId)
      this._usedPlaceIds.add(match.placeId);

    let index = 0;
    // The buckets change depending on the context, that is currently decided by
    // the first added match (the heuristic one).
    if (!this._buckets) {
      // Convert the buckets to readable objects with a count property.
      let buckets = match.type == UrlbarUtils.MATCH_GROUP.HEURISTIC &&
                    match.style.includes("searchengine") ? UrlbarPrefs.get("matchBucketsSearch")
                                                         : UrlbarPrefs.get("matchBuckets");
      // - available is the number of available slots in the bucket
      // - insertIndex is the index of the first available slot in the bucket
      // - count is the number of matches in the bucket, note that it also
      //   account for matches from the previous search, while available and
      //   insertIndex don't.
      this._buckets = buckets.map(([type, available]) => ({ type,
                                                            available,
                                                            insertIndex: 0,
                                                            count: 0,
                                                          }));

      // If we have matches from the previous search, we want to replace them
      // in-place to reduce flickering.
      // This requires walking the previous matches and marking their existence
      // into the current buckets, so that, when we'll add the new matches to
      // the buckets, we can either append or replace a match.
      if (this._previousSearchMatchTypes.length > 0) {
        for (let type of this._previousSearchMatchTypes) {
          for (let bucket of this._buckets) {
            if (type == bucket.type && bucket.count < bucket.available) {
              bucket.count++;
              break;
            }
          }
        }
      }
    }

    let replace = 0;
    for (let bucket of this._buckets) {
      // Move to the next bucket if the match type is incompatible, or if there
      // is no available space or if the frecency is below the threshold.
      if (match.type != bucket.type || !bucket.available) {
        index += bucket.count;
        continue;
      }

      index += bucket.insertIndex;
      bucket.available--;
      if (bucket.insertIndex < bucket.count) {
        replace = true;
      } else {
        bucket.count++;
      }
      bucket.insertIndex++;
      break;
    }
    this._usedURLs[index] = {key: urlMapKey, action, type: match.type};
    return { index, replace };
  },

  /**
   * Removes matches from a previous search, that are no more returned by the
   * current search
   * @param type
   *        The UrlbarUtils.MATCH_GROUP to clean up.  Pass null (or another
   *        falsey value) to clean up all groups.
   * @param [optional] notify
   *        Whether to notify a result change.
   */
  _cleanUpNonCurrentMatches(type, notify = true) {
    if (this._previousSearchMatchTypes.length == 0 || !this.pending)
      return;

    let index = 0;
    let changed = false;
    if (!this._buckets) {
      // No match arrived yet, so any match of the given type should be removed
      // from the top.
      while (this._previousSearchMatchTypes.length &&
             (!type || this._previousSearchMatchTypes[0] == type)) {
        this._previousSearchMatchTypes.shift();
        this._result.removeMatchAt(0);
        changed = true;
      }
    } else {
      for (let bucket of this._buckets) {
        if (type && bucket.type != type) {
          index += bucket.count;
          continue;
        }
        index += bucket.insertIndex;

        while (bucket.count > bucket.insertIndex) {
          this._result.removeMatchAt(index);
          changed = true;
          bucket.count--;
        }
      }
    }
    if (changed && notify) {
      this.notifyResult(true);
    }
  },

  /**
   * If in restrict mode, cleanups non current matches for all the empty types.
   */
  cleanUpRestrictNonCurrentMatches() {
    if (this.hasBehavior("restrict") && this._previousSearchMatchTypes.length > 0) {
      for (let type of new Set(this._previousSearchMatchTypes)) {
        if (this._counts[type] == 0) {
          // Don't notify, since we are about to notify completion.
          this._cleanUpNonCurrentMatches(type, false);
        }
      }
    }
  },

  _addOriginAutofillMatch(row) {
    this._addAutofillMatch(
      row.getResultByIndex(QUERYINDEX_ORIGIN_AUTOFILLED_VALUE),
      row.getResultByIndex(QUERYINDEX_ORIGIN_URL),
      row.getResultByIndex(QUERYINDEX_ORIGIN_FRECENCY)
    );
  },

  _addURLAutofillMatch(row) {
    let url = row.getResultByIndex(QUERYINDEX_URL_URL);
    let strippedURL = row.getResultByIndex(QUERYINDEX_URL_STRIPPED_URL);
    // We autofill urls to-the-next-slash.
    // http://mozilla.org/foo/bar/baz will be autofilled to:
    //  - http://mozilla.org/f[oo/]
    //  - http://mozilla.org/foo/b[ar/]
    //  - http://mozilla.org/foo/bar/b[az]
    let value;
    let strippedURLIndex = url.indexOf(strippedURL);
    let strippedPrefix = url.substr(0, strippedURLIndex);
    let nextSlashIndex = url.indexOf("/", strippedURLIndex + strippedURL.length - 1);
    if (nextSlashIndex == -1) {
      value = url.substr(strippedURLIndex);
    } else {
      value = url.substring(strippedURLIndex, nextSlashIndex + 1);
    }

    this._addAutofillMatch(
      value,
      strippedPrefix + value,
      row.getResultByIndex(QUERYINDEX_URL_FRECENCY)
    );
  },

  _addAutofillMatch(autofilledValue, finalCompleteValue, frecency = Infinity, extraStyles = []) {
    // The match's comment is only for display.  Set it to finalCompleteValue,
    // the actual URL that will be visited when the user chooses the match, so
    // that the user knows exactly where the match will take them.  To make it
    // look a little nicer, remove "http://", and if the user typed a host
    // without a trailing slash, remove any trailing slash, too.
    let comment = stripHttpAndTrim(finalCompleteValue,
                                   !this._searchString.includes("/"));
    this._addMatch({
      value: this._strippedPrefix + autofilledValue,
      finalCompleteValue,
      comment,
      frecency,
      style: ["autofill"].concat(extraStyles).join(" "),
      icon: iconHelper(finalCompleteValue),
    });
  },

  // This is the same as _addFilteredQueryMatch, but it only returns a few
  // results, caching the others. If at the end we don't find other results, we
  // can add these.
  _addAdaptiveQueryMatch(row) {
    // Allow one quarter of the results to be adaptive results.
    // Note: ideally adaptive results should have their own provider and the
    // results muxer should decide what to show.  But that's too complex to
    // support in the current code, so that's left for a future refactoring.
    if (this._adaptiveCount < Math.ceil(UrlbarPrefs.get("maxRichResults") / 4)) {
      this._addFilteredQueryMatch(row);
    } else {
      this._extraAdaptiveRows.push(row);
    }
    this._adaptiveCount++;
  },

  _addFilteredQueryMatch(row) {
    let match = {};
    match.placeId = row.getResultByIndex(QUERYINDEX_PLACEID);
    let escapedURL = row.getResultByIndex(QUERYINDEX_URL);
    let openPageCount = row.getResultByIndex(QUERYINDEX_SWITCHTAB) || 0;
    let historyTitle = row.getResultByIndex(QUERYINDEX_TITLE) || "";
    let bookmarked = row.getResultByIndex(QUERYINDEX_BOOKMARKED);
    let bookmarkTitle = bookmarked ?
      row.getResultByIndex(QUERYINDEX_BOOKMARKTITLE) : null;
    let tags = row.getResultByIndex(QUERYINDEX_TAGS) || "";
    let frecency = row.getResultByIndex(QUERYINDEX_FRECENCY);

    // If actions are enabled and the page is open, add only the switch-to-tab
    // result.  Otherwise, add the normal result.
    let url = escapedURL;
    let action = null;
    if (this._enableActions && openPageCount > 0 && this.hasBehavior("openpage")) {
      url = PlacesUtils.mozActionURI("switchtab", {url: escapedURL});
      action = "switchtab";
      if (frecency == null)
        frecency = FRECENCY_DEFAULT;
    }

    // Always prefer the bookmark title unless it is empty
    let title = bookmarkTitle || historyTitle;

    // We will always prefer to show tags if we have them.
    let showTags = !!tags;

    // However, we'll act as if a page is not bookmarked if the user wants
    // only history and not bookmarks and there are no tags.
    if (this.hasBehavior("history") && !this.hasBehavior("bookmark") &&
        !showTags) {
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
        // If we're not suggesting bookmarks, then this shouldn't
        // display as one.
        match.style = this.hasBehavior("bookmark") ? "bookmark-tag" : "tag";
      } else if (bookmarked) {
        match.style = "bookmark";
      }
    }

    if (action)
      match.style = "action " + action;

    match.value = url;
    match.comment = title;
    match.icon = iconHelper(escapedURL);
    match.frecency = frecency;

    this._addMatch(match);
  },

  /**
   * @return a string consisting of the search query to be used based on the
   * previously set urlbar suggestion preferences.
   */
  get _suggestionPrefQuery() {
    if (!this.hasBehavior("restrict") && this.hasBehavior("history") &&
        this.hasBehavior("bookmark")) {
      return defaultQuery();
    }
    let conditions = [];
    if (this.hasBehavior("history")) {
      // Enforce ignoring the visit_count index, since the frecency one is much
      // faster in this case.  ANALYZE helps the query planner to figure out the
      // faster path, but it may not have up-to-date information yet.
      conditions.push("+h.visit_count > 0");
    }
    if (this.hasBehavior("bookmark")) {
      conditions.push("bookmarked");
    }
    if (this.hasBehavior("tag")) {
      conditions.push("tags NOTNULL");
    }

    return conditions.length ? defaultQuery("AND " + conditions.join(" AND "))
                             : defaultQuery();
  },

  /**
   * Get the search string with the keyword substitution applied.
   * If the user-provided string starts with a keyword that gave a heuristic
   * result, it can provide a substitute string (e.g. the domain that keyword
   * will search) so that the history/bookmark results we show will correspond
   * to the keyword search rather than searching for the literal keyword.
   */
  get _keywordSubstitutedSearchString() {
    let tokens = this._searchTokens;
    if (this._keywordSubstitute) {
      tokens = [this._keywordSubstitute, ...this._searchTokens.slice(1)];
    }
    return tokens.join(" ");
  },

  /**
   * Obtains the search query to be used based on the previously set search
   * preferences (accessed by this.hasBehavior).
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _searchQuery() {
    let query = this._suggestionPrefQuery;

    return [
      query,
      {
        parent: PlacesUtils.tagsFolderId,
        query_type: QUERYTYPE_FILTERED,
        matchBehavior: this._matchBehavior,
        searchBehavior: this._behavior,
        // We only want to search the tokens that we are left with - not the
        // original search string.
        searchString: this._keywordSubstitutedSearchString,
        userContextId: this._userContextId,
        // Limit the query to the the maximum number of desired results.
        // This way we can avoid doing more work than needed.
        maxResults: UrlbarPrefs.get("maxRichResults"),
      },
    ];
  },

  /**
   * Obtains the query to search for switch-to-tab entries.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _switchToTabQuery() {
    return [
      SQL_SWITCHTAB_QUERY,
      {
        query_type: QUERYTYPE_FILTERED,
        matchBehavior: this._matchBehavior,
        searchBehavior: this._behavior,
        // We only want to search the tokens that we are left with - not the
        // original search string.
        searchString: this._keywordSubstitutedSearchString,
        userContextId: this._userContextId,
        maxResults: UrlbarPrefs.get("maxRichResults"),
      },
    ];
  },

  /**
   * Obtains the query to search for adaptive results.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _adaptiveQuery() {
    return [
      SQL_ADAPTIVE_QUERY,
      {
        parent: PlacesUtils.tagsFolderId,
        search_string: this._searchString,
        query_type: QUERYTYPE_ADAPTIVE,
        matchBehavior: this._matchBehavior,
        searchBehavior: this._behavior,
        userContextId: this._userContextId,
        maxResults: UrlbarPrefs.get("maxRichResults"),
      },
    ];
  },

  /**
   * Whether we should try to autoFill.
   */
  get _shouldAutofill() {
    // First of all, check for the autoFill pref.
    if (!UrlbarPrefs.get("autoFill"))
      return false;

    if (this._searchTokens.length != 1)
      return false;

    // autoFill can only cope with history, bookmarks, and about: entries.
    if (!this.hasBehavior("history") &&
        !this.hasBehavior("bookmark"))
      return false;

    // autoFill doesn't search titles or tags.
    if (this.hasBehavior("title") || this.hasBehavior("tag"))
      return false;

    // Don't try to autofill if the search term includes any whitespace.
    // This may confuse completeDefaultIndex cause the AUTOCOMPLETE_MATCH
    // tokenizer ends up trimming the search string and returning a value
    // that doesn't match it, or is even shorter.
    if (REGEXP_SPACES.test(this._originalSearchString))
      return false;

    if (this._searchString.length == 0)
      return false;

    if (this._prohibitAutoFill)
      return false;

    return true;
  },

  /**
   * Obtains the query to search for autofill origin results.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _originQuery() {
    // At this point, _searchString is not a URL with a path; it does not
    // contain a slash, except for possibly at the very end.  If there is
    // trailing slash, remove it when searching here to match the rest of the
    // string because it may be an origin.
    let searchStr =
      this._searchString.endsWith("/") ?
      this._searchString.slice(0, -1) :
      this._searchString;

    let opts = {
      query_type: QUERYTYPE_AUTOFILL_ORIGIN,
      searchString: searchStr.toLowerCase(),
      stddevMultiplier: UrlbarPrefs.get("autoFill.stddevMultiplier"),
    };

    let bookmarked = this.hasBehavior("bookmark") &&
                     !this.hasBehavior("history");
    if (this._strippedPrefix) {
      opts.prefix = this._strippedPrefix;
      if (bookmarked) {
        return [SQL_ORIGIN_PREFIX_BOOKMARKED_QUERY, opts];
      }
      return [SQL_ORIGIN_PREFIX_QUERY, opts];
    }
    if (bookmarked) {
      return [SQL_ORIGIN_BOOKMARKED_QUERY, opts];
    }
    return [SQL_ORIGIN_QUERY, opts];
  },

  /**
   * Obtains the query to search for autoFill url results.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _urlQuery() {
    // Try to get the host from the search string.  The host is the part of the
    // URL up to either the path slash, port colon, or query "?".  If the search
    // string doesn't look like it begins with a host, then return; it doesn't
    // make sense to do a URL query with it.
    if (!this._urlQueryHostRegexp) {
      this._urlQueryHostRegexp = /^[^/:?]+/;
    }
    let hostMatch = this._urlQueryHostRegexp.exec(this._searchString);
    if (!hostMatch) {
      return [null, null];
    }

    let host = hostMatch[0].toLowerCase();
    let revHost = host.split("").reverse().join("") + ".";

    // Build a string that's the URL stripped of its prefix, i.e., the host plus
    // everything after the host.  Use _trimmedOriginalSearchString instead of
    // this._searchString because this._searchString has had unEscapeURIForUI()
    // called on it.  It's therefore not necessarily the literal URL.
    let strippedURL = this._trimmedOriginalSearchString;
    if (this._strippedPrefix) {
      strippedURL = strippedURL.substr(this._strippedPrefix.length);
    }
    strippedURL = host + strippedURL.substr(host.length);

    let opts = {
      query_type: QUERYTYPE_AUTOFILL_URL,
      revHost,
      strippedURL,
    };

    let bookmarked = this.hasBehavior("bookmark") &&
                     !this.hasBehavior("history");

    if (this._strippedPrefix) {
      opts.prefix = this._strippedPrefix;
      if (bookmarked) {
        return [SQL_URL_PREFIX_BOOKMARKED_QUERY, opts];
      }
      return [SQL_URL_PREFIX_QUERY, opts];
    }
    if (bookmarked) {
      return [SQL_URL_BOOKMARKED_QUERY, opts];
    }
    return [SQL_URL_QUERY, opts];
  },

  // The result is notified to the search listener on a timer, to chunk multiple
  // match updates together and avoid rebuilding the popup at every new match.
  _notifyTimer: null,

  /**
   * Notifies the current result to the listener.
   *
   * @param searchOngoing
   *        Indicates whether the search result should be marked as ongoing.
   * @param skipDelay
   *        Whether to notify immediately.
   */
  _notifyDelaysCount: 0,
  notifyResult(searchOngoing, skipDelay = false) {
    let notify = () => {
      if (!this.pending)
        return;
      this._notifyDelaysCount = 0;
      let resultCode = this._currentMatchCount ? "RESULT_SUCCESS" : "RESULT_NOMATCH";
      if (searchOngoing) {
        resultCode += "_ONGOING";
      }
      let result = this._result;
      result.setSearchResult(Ci.nsIAutoCompleteResult[resultCode]);
      this._listener.onSearchResult(this._autocompleteSearch, result);
      if (!searchOngoing) {
        // Break possible cycles.
        this._listener = null;
        this._autocompleteSearch = null;
        this.stop();
      }
    };
    if (this._notifyTimer) {
      this._notifyTimer.cancel();
    }
    // In the worst case, we may get evenly spaced matches that would end up
    // delaying the UI by N_MATCHES * NOTIFYRESULT_DELAY_MS. Thus, we clamp the
    // number of times we may delay matches.
    if (skipDelay || this._notifyDelaysCount > 3) {
      notify();
    } else {
      this._notifyDelaysCount++;
      this._notifyTimer = setTimeout(notify, NOTIFYRESULT_DELAY_MS);
    }
  },
};

// UnifiedComplete class
// component @mozilla.org/autocomplete/search;1?name=unifiedcomplete

function UnifiedComplete() {
  if (UrlbarPrefs.get("usepreloadedtopurls.enabled")) {
    // force initializing the profile age check
    // to ensure the off-main-thread-IO happens ASAP
    // and we don't have to wait for it when doing an autocomplete lookup
    ProfileAgeCreatedPromise;

    fetch("chrome://global/content/unifiedcomplete-top-urls.json")
      .then(response => response.json())
      .then(sites => PreloadedSiteStorage.populate(sites))
      .catch(ex => Cu.reportError(ex));
  }
}

UnifiedComplete.prototype = {
  // Database handling

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
  getDatabaseHandle() {
    if (!this._promiseDatabase) {
      this._promiseDatabase = (async () => {
        let conn = await PlacesUtils.promiseLargeCacheDBConnection();

        try {
           Sqlite.shutdown.addBlocker("Places UnifiedComplete.js closing",
                                      () => {
                                        // Break a possible cycle through the
                                        // previous result, the controller and
                                        // ourselves.
                                        this._currentSearch = null;
                                      });
        } catch (ex) {
          // It's too late to block shutdown.
          throw ex;
        }
        await UrlbarProviderOpenTabs.promiseDb();
        return conn;
      })().catch(ex => {
        dump("Couldn't get database handle: " + ex + "\n");
        Cu.reportError(ex);
      });
    }
    return this._promiseDatabase;
  },

  // mozIPlacesAutoComplete

  populatePreloadedSiteStorage(json) {
    PreloadedSiteStorage.populate(json);
  },

  // nsIAutoCompleteSearch

  startSearch(searchString, searchParam, acPreviousResult, listener) {
    // Stop the search in case the controller has not taken care of it.
    if (this._currentSearch) {
      this.stopSearch();
    }

    // If the previous search didn't fetch enough search suggestions, it's
    // unlikely a longer text would do.
    let prohibitSearchSuggestions =
      !!this._lastLowResultsSearchSuggestion &&
      searchString.length > this._lastLowResultsSearchSuggestion.length &&
      searchString.startsWith(this._lastLowResultsSearchSuggestion);

    // We don't directly reuse the controller provided previousResult because:
    //  * it is only populated when the new searchString is an extension of the
    //    previous one. We want to handle the backspace case too.
    //  * Bookmarks may be titled differently than history and we want to show
    //    the right title.  For example a "foo" titled page could be bookmarked
    //    as "foox", typing "foo" followed by "x" would show the history result
    //    from the previous search (See bug 412730).
    //  * Adaptive History means a result may appear even if the previous string
    //    didn't match it.
    // What we can do is reuse the previous result along with the bucketing
    // system to avoid flickering. Since we know where a new match should be
    // positioned, we  wait for a new match to arrive before replacing the
    // previous one. This may leave stale matches from the previous search that
    // would not be returned by the current one, thus once the current search is
    // complete, we remove those stale matches with _cleanUpNonCurrentMatches().
    let previousResult = null;
    let insertMethod = UrlbarPrefs.get("insertMethod");
    if (this._currentSearch && insertMethod != UrlbarUtils.INSERTMETHOD.APPEND) {
      let result = this._currentSearch._result;
      // Only reuse the previous result when one of the search strings is an
      // extension of the other one.  We could expand this to any case, but
      // that may leave exposed unrelated matches for a longer time.
      let previousSearchString = result.searchString;
      let stringsRelated = previousSearchString.length > 0 &&
                           searchString.length > 0 &&
                           (previousSearchString.includes(searchString) ||
                            searchString.includes(previousSearchString));
      if (insertMethod == UrlbarUtils.INSERTMETHOD.MERGE || stringsRelated) {
        previousResult = result;
      }
    }

    let search = this._currentSearch = new Search(searchString, searchParam,
                                                  listener, this,
                                                  prohibitSearchSuggestions,
                                                  previousResult);
    this.getDatabaseHandle().then(conn => search.execute(conn))
                            .catch(ex => {
                              dump(`Query failed: ${ex}\n`);
                              Cu.reportError(ex);
                            })
                            .then(() => {
                              if (search == this._currentSearch) {
                                this.finishSearch(true);
                              }
                            });
  },

  stopSearch() {
    if (this._currentSearch) {
      this._currentSearch.stop();
    }
    // Don't notify since we are canceling this search.  This also means we
    // won't fire onSearchComplete for this search.
    this.finishSearch();
  },

  /**
   * Properly cleans up when searching is completed.
   *
   * @param notify [optional]
   *        Indicates if we should notify the AutoComplete listener about our
   *        results or not.
   */
  finishSearch(notify = false) {
    TelemetryStopwatch.cancel(TELEMETRY_1ST_RESULT, this);
    TelemetryStopwatch.cancel(TELEMETRY_6_FIRST_RESULTS, this);
    // Clear state now to avoid race conditions, see below.
    let search = this._currentSearch;
    if (!search)
      return;
    this._lastLowResultsSearchSuggestion = search._lastLowResultsSearchSuggestion;

    if (!notify || !search.pending)
      return;

    // If we are in restrict mode and we reused the previous search results,
    // it's possible we didn't go through all the cleanup methods due to early
    // bailouts. Thus we could still have nonmatching results to remove.
    search.cleanUpRestrictNonCurrentMatches();

    // There is a possible race condition here.
    // When a search completes it calls finishSearch that notifies results
    // here.  When the controller gets the last result it fires
    // onSearchComplete.
    // If onSearchComplete immediately starts a new search it will set a new
    // _currentSearch, and on return the execution will continue here, after
    // notifyResult.
    // Thus, ensure that notifyResult is the last call in this method,
    // otherwise you might be touching the wrong search.
    search.notifyResult(false);
  },

  // nsIAutoCompleteSearchDescriptor

  get searchType() {
    return Ci.nsIAutoCompleteSearchDescriptor.SEARCH_TYPE_IMMEDIATE;
  },

  get clearingAutoFillSearchesAgain() {
    return true;
  },

  // nsISupports

  classID: Components.ID("f964a319-397a-4d21-8be6-5cdd1ee3e3ae"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(UnifiedComplete),

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIAutoCompleteSearch,
    Ci.nsIAutoCompleteSearchDescriptor,
    Ci.mozIPlacesAutoComplete,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([UnifiedComplete]);
