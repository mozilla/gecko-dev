/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu, Ci, Cc } = require("chrome");
const { defer, all, resolve } = require("sdk/core/promise");
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

loader.lazyImporter(this, "ViewHelpers", "resource:///modules/devtools/ViewHelpers.jsm");
loader.lazyRequireGetter(this, "NetworkHelper", "devtools/toolkit/webconsole/network-helper");

loader.lazyGetter(this, "appInfo", () => {
  return Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);
});

loader.lazyGetter(this, "L10N", () => {
  return new ViewHelpers.L10N("chrome://browser/locale/devtools/har.properties");
});

const HAR_VERSION = "1.1";

/**
 * This object is responsible for building HAR file. See HAR spec:
 * https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/HAR/Overview.html
 * http://www.softwareishard.com/blog/har-12-spec/
 *
 * @param {Object} options configuration object
 *
 * The following options are supported:
 *
 * - items {Array}: List of Network requests to be exported. It is possible
 *   to use directly: NetMonitorView.RequestsMenu.items
 *
 * - id {String}: ID of the exported page.
 *
 * - title {String}: Title of the exported page.
 *
 * - includeResponseBodies {Boolean}: Set to true to include HTTP response
 *   bodies in the result data structure.
 */
var HarBuilder = function(options) {
  this._options = options;
  this._pageMap = [];
}

HarBuilder.prototype = {
  // Public API

  /**
   * This is the main method used to build the entire result HAR data.
   * The process is asynchronous since it can involve additional RDP
   * communication (e.g. resolving long strings).
   *
   * @returns {Promise} A promise that resolves to the HAR object when
   * the entire build process is done.
   */
  build: function() {
    this.promises = [];

    // Build basic structure for data.
    let log = this.buildLog();

    // Build entries.
    let items = this._options.items;
    for (let i=0; i<items.length; i++) {
      let file = items[i].attachment;
      log.entries.push(this.buildEntry(log, file));
    }

    // Some data needs to be fetched from the backend during the
    // build process, so wait till all is done.
    let { resolve, promise } = defer();
    all(this.promises).then(results => resolve({ log: log }));

    return promise;
  },

  // Helpers

  buildLog: function() {
    return {
      version: HAR_VERSION,
      creator: {
        name: appInfo.name,
        version: appInfo.version
      },
      browser: {
        name: appInfo.name,
        version: appInfo.version
      },
      pages: [],
      entries: [],
    }
  },

  buildPage: function(file) {
    let page = {};

    // Page start time is set when the first request is processed
    // (see buildEntry)
    page.startedDateTime = 0;
    page.id = "page_" + this._options.id;
    page.title = this._options.title;

    return page;
  },

  getPage: function(log, file) {
    let id = this._options.id;
    let page = this._pageMap[id];
    if (page) {
      return page;
    }

    this._pageMap[id] = page = this.buildPage(file);
    log.pages.push(page);

    return page;
  },

  buildEntry: function(log, file) {
    let page = this.getPage(log, file);

    let entry = {};
    entry.pageref = page.id;
    entry.startedDateTime = dateToJSON(new Date(file.startedMillis));
    entry.time = file.endedMillis - file.startedMillis;

    entry.request = this.buildRequest(file);
    entry.response = this.buildResponse(file);
    entry.cache = this.buildCache(file);
    entry.timings = file.eventTimings ? file.eventTimings.timings : {};

    if (file.remoteAddress) {
      entry.serverIPAddress = file.remoteAddress;
    }

    if (file.remotePort) {
      entry.connection = file.remotePort + "";
    }

    // Compute page load start time according to the first request start time.
    if (!page.startedDateTime) {
      page.startedDateTime = entry.startedDateTime;
      page.pageTimings = this.buildPageTimings(page, file);
    }

    return entry;
  },

  buildPageTimings: function(page, file) {
    // Event timing info isn't available
    let timings = {
      onContentLoad: -1,
      onLoad: -1
    };

    return timings;
  },

  buildRequest: function(file) {
    let request = {
      bodySize: 0
    };

    request.method = file.method;
    request.url = file.url;
    request.httpVersion = file.httpVersion;

    request.headers = this.buildHeaders(file.requestHeaders);
    request.cookies = this.buildCookies(file.requestCookies);

    request.queryString = NetworkHelper.parseQueryString(
      NetworkHelper.nsIURL(file.url).query) || [];

    request.postData = this.buildPostData(file);

    request.headersSize = file.requestHeaders.headersSize;

    // Set request body size, but make sure the body is fetched
    // from the backend.
    if (file.requestPostData) {
      this.fetchData(file.requestPostData.postData.text).then(value => {
        request.bodySize = value.length;
      });
    }

    return request;
  },

  /**
   * Fetch all header values from the backend (if necessary) and
   * build the result HAR structure.
   *
   * @param {Object} input Request or response header object.
   */
  buildHeaders: function(input) {
    if (!input) {
      return [];
    }

    return this.buildNameValuePairs(input.headers);
  },

  buildCookies: function(input) {
    if (!input) {
      return [];
    }

    return this.buildNameValuePairs(input.cookies);
  },

  buildNameValuePairs: function(entries) {
    let result = [];

    // HAR requires headers array to be presented, so always
    // return at least an empty array.
    if (!entries) {
      return result;
    }

    // Make sure header values are fully fetched from the server.
    entries.forEach(entry => {
      this.fetchData(entry.value).then(value => {
        result.push({
          name: entry.name,
          value: value
        });
      });
    })

    return result;
  },

  buildPostData: function(file) {
    let postData = {
      mimeType: findValue(file.requestHeaders.headers, "content-type"),
      params: [],
      text: ""
    };

    if (!file.requestPostData) {
      return postData;
    }

    if (file.requestPostData.postDataDiscarded) {
      postData.comment = L10N.getStr("har.requestBodyNotIncluded");
      return postData;
    }

    // Load request body from the backend.
    this.fetchData(file.requestPostData.postData.text).then(value => {
      postData.text = value;

      // If we are dealing with URL encoded body, parse parameters.
      if (isURLEncodedFile(file, value)) {
        postData.mimeType = "application/x-www-form-urlencoded";

        // Extract form parameters and produce nice HAR array.
        this._options.view._getFormDataSections(file.requestHeaders,
          file.requestHeadersFromUploadStream,
          file.requestPostData).then(formDataSections => {
            formDataSections.forEach(section => {
              let paramsArray = NetworkHelper.parseQueryString(section);
              if (paramsArray) {
                postData.params = [...postData.params, ...paramsArray];
              }
            });
          });
      }
    });

    return postData;
  },

  buildResponse: function(file) {
    let response = {
      status: 0
    };

    // Arbitrary value if it's aborted to make sure status has a number
    if (file.status) {
      response.status = parseInt(file.status);
    }

    response.statusText = file.statusText || "";
    response.httpVersion = file.httpVersion;

    response.headers = this.buildHeaders(file.responseHeaders);
    response.cookies = this.buildCookies(file.responseCookies);

    response.content = this.buildContent(file);
    response.redirectURL = findValue(file.responseHeaders.headers, "Location");
    response.headersSize = file.responseHeaders.headersSize;
    response.bodySize = file.transferredSize || -1;

    return response;
  },

  buildContent: function(file) {
    let content = {
      mimeType: file.mimeType,
      size: -1
    };

    if (file.responseContent && file.responseContent.content) {
      content.size = file.responseContent.content.size;
    }

    if (!this._options.includeResponseBodies ||
        file.responseContent.contentDiscarded) {
      content.comment = L10N.getStr("har.responseBodyNotIncluded");
      return content;
    }

    if (file.responseContent) {
      let text = file.responseContent.content.text;
      let promise = this.fetchData(text).then(value => {
        content.text = value;
      });
    }

    return content;
  },

  buildCache: function(file) {
    let cache = {};

    if (!file.fromCache) {
      return cache;
    }

    // There is no such info yet in the Net panel.
    // cache.beforeRequest = {};

    if (file.cacheEntry) {
      cache.afterRequest = this.buildCacheEntry(file.cacheEntry);
    } else {
      cache.afterRequest = null;
    }

    return cache;
  },

  buildCacheEntry: function(cacheEntry) {
    let cache = {};

    cache.expires = findValue(cacheEntry, "Expires");
    cache.lastAccess = findValue(cacheEntry, "Last Fetched");
    cache.eTag = "";
    cache.hitCount = findValue(cacheEntry, "Fetch Count");

    return cache;
  },

  getBlockingEndTime: function(file) {
    if (file.resolveStarted && file.connectStarted) {
      return file.resolvingTime;
    }

    if (file.connectStarted) {
      return file.connectingTime;
    }

    if (file.sendStarted) {
      return file.sendingTime;
    }

    return (file.sendingTime > file.startTime) ?
      file.sendingTime : file.waitingForTime;
  },

  // RDP Helpers

  fetchData: function(string) {
    let promise = this._options.getString(string).then(value => {
      return value;
    });

    // Building HAR is asynchronous and not done till all
    // collected promises are resolved.
    this.promises.push(promise);

    return promise;
  }
}

// Helpers

/**
 * Returns true if specified request body is URL encoded.
 */
function isURLEncodedFile(file, text) {
  let contentType = "content-type: application/x-www-form-urlencoded"
  if (text && text.toLowerCase().indexOf(contentType) != -1) {
    return true;
  }

  // The header value doesn't have to be always exactly
  // "application/x-www-form-urlencoded",
  // there can be even charset specified. So, use indexOf rather than just
  // "==".
  let value = findValue(file.requestHeaders.headers, "content-type");
  if (value && value.indexOf("application/x-www-form-urlencoded") == 0) {
    return true;
  }

  return false;
}

/**
 * Find specified value within an array of name-value pairs
 * (used for headers, cookies and cache entries)
 */
function findValue(arr, name) {
  name = name.toLowerCase();
  let result = arr.find(entry => entry.name.toLowerCase() == name);
  return result ? result.value : "";
}

/**
 * Generate HAR representation of a date.
 * (YYYY-MM-DDThh:mm:ss.sTZD, e.g. 2009-07-24T19:20:30.45+01:00)
 * See also HAR Schema: http://janodvarko.cz/har/viewer/
 *
 * Note: it would be great if we could utilize Date.toJSON(), but
 * it doesn't return proper time zone offset.
 *
 * An example:
 * This helper returns:    2015-05-29T16:10:30.424+02:00
 * Date.toJSON() returns:  2015-05-29T14:10:30.424Z
 *
 * @param date {Date} The date object we want to convert.
 */
function dateToJSON(date) {
  function f(n, c) {
    if (!c) {
      c = 2;
    }
    let s = new String(n);
    while (s.length < c) {
      s = "0" + s;
    }
    return s;
  }

  let result = date.getFullYear() + '-' +
    f(date.getMonth() + 1) + '-' +
    f(date.getDate()) + 'T' +
    f(date.getHours()) + ':' +
    f(date.getMinutes()) + ':' +
    f(date.getSeconds()) + '.' +
    f(date.getMilliseconds(), 3);

  let offset = date.getTimezoneOffset();
  let positive = offset > 0;

  // Convert to positive number before using Math.floor (see issue 5512)
  offset = Math.abs(offset);
  let offsetHours = Math.floor(offset / 60);
  let offsetMinutes = Math.floor(offset % 60);
  let prettyOffset = (positive > 0 ? "-" : "+") + f(offsetHours) +
    ":" + f(offsetMinutes);

  return result + prettyOffset;
}

// Exports from this module
exports.HarBuilder = HarBuilder;
