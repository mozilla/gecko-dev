/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  NetworkHelper:
    "resource://devtools/shared/network-observer/NetworkHelper.sys.mjs",
  NetworkUtils:
    "resource://devtools/shared/network-observer/NetworkUtils.sys.mjs",

  notifyNavigationStarted:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
});

/**
 * The NetworkRequest class is a wrapper around the internal channel which
 * provides getters and methods closer to fetch's response concept
 * (https://fetch.spec.whatwg.org/#concept-response).
 */
export class NetworkRequest {
  #alreadyCompleted;
  #channel;
  #contextId;
  #eventRecord;
  #isDataURL;
  #navigationId;
  #navigationManager;
  #rawHeaders;
  #redirectCount;
  #requestId;
  #timedChannel;
  #wrappedChannel;

  /**
   *
   * @param {nsIChannel} channel
   *     The channel for the request.
   * @param {object} params
   * @param {NetworkEventRecord} params.networkEventRecord
   *     The NetworkEventRecord owning this NetworkRequest.
   * @param {NavigationManager} params.navigationManager
   *     The NavigationManager where navigations for the current session are
   *     monitored.
   * @param {string=} params.rawHeaders
   *     The request's raw (ie potentially compressed) headers
   */
  constructor(channel, params) {
    const { eventRecord, navigationManager, rawHeaders = "" } = params;

    this.#channel = channel;
    this.#eventRecord = eventRecord;
    this.#isDataURL = this.#channel instanceof Ci.nsIDataChannel;
    this.#navigationManager = navigationManager;
    this.#rawHeaders = rawHeaders;

    const currentTime = Date.now();
    this.#timedChannel =
      this.#channel instanceof Ci.nsITimedChannel
        ? this.#channel.QueryInterface(Ci.nsITimedChannel)
        : {
            redirectCount: 0,
            channelCreationTime: currentTime,
            redirectStartTime: 0,
            redirectEndTime: 0,
            domainLookupStartTime: currentTime,
            domainLookupEndTime: currentTime,
            connectStartTime: currentTime,
            connectEndTime: currentTime,
            secureConnectionStartTime: currentTime,
            requestStartTime: currentTime,
            responseStartTime: currentTime,
            responseEndTime: currentTime,
          };
    this.#wrappedChannel = ChannelWrapper.get(channel);

    this.#redirectCount = this.#timedChannel.redirectCount;
    // The wrappedChannel id remains identical across redirects, whereas
    // nsIChannel.channelId is different for each and every request.
    this.#requestId = this.#wrappedChannel.id.toString();

    this.#contextId = this.#getContextId();
    this.#navigationId = this.#getNavigationId();
  }

  get alreadyCompleted() {
    return this.#alreadyCompleted;
  }

  get channel() {
    return this.#channel;
  }

  get contextId() {
    return this.#contextId;
  }

  get errorText() {
    // TODO: Update with a proper error text. Bug 1873037.
    return ChromeUtils.getXPCOMErrorName(this.#channel.status);
  }

  get headers() {
    return this.#getHeadersList();
  }

  get headersSize() {
    // TODO: rawHeaders will not be updated after modifying the headers via
    // request interception. Need to find another way to retrieve the
    // information dynamically.
    return this.#rawHeaders.length;
  }

  get isHttpChannel() {
    return this.#channel instanceof Ci.nsIHttpChannel;
  }

  get method() {
    return this.#isDataURL ? "GET" : this.#channel.requestMethod;
  }

  get navigationId() {
    return this.#navigationId;
  }

  get postDataSize() {
    const charset = lazy.NetworkUtils.getCharset(this.#channel);
    const sentBody = lazy.NetworkHelper.readPostTextFromRequest(
      this.#channel,
      charset
    );
    return sentBody ? sentBody.length : 0;
  }

  get redirectCount() {
    return this.#redirectCount;
  }

  get requestId() {
    return this.#requestId;
  }

  get serializedURL() {
    return this.#channel.URI.spec;
  }

  get supportsInterception() {
    // The request which doesn't have `wrappedChannel` can not be intercepted.
    return !!this.#wrappedChannel;
  }

  get timings() {
    return this.#getFetchTimings();
  }

  get wrappedChannel() {
    return this.#wrappedChannel;
  }

  set alreadyCompleted(value) {
    this.#alreadyCompleted = value;
  }

  /**
   * Add information about raw headers, collected from NetworkObserver events.
   *
   * @param {string} rawHeaders
   *     The raw headers.
   */
  addRawHeaders(rawHeaders) {
    this.#rawHeaders = rawHeaders || "";
  }

  /**
   * Clear a request header from the request's headers list.
   *
   * @param {string} name
   *     The header's name.
   */
  clearRequestHeader(name) {
    this.#channel.setRequestHeader(
      name, // aName
      "", // aValue="" as an empty value
      false // aMerge=false to force clearing the header
    );
  }

  /**
   * Set the request post body
   *
   * @param {string} body
   *     The body to set.
   */
  setRequestBody(body) {
    // Update the requestObserversCalled flag to allow modifying the request,
    // and reset once done.
    this.#channel.requestObserversCalled = false;

    try {
      this.#channel.QueryInterface(Ci.nsIUploadChannel2);
      const bodyStream = Cc[
        "@mozilla.org/io/string-input-stream;1"
      ].createInstance(Ci.nsIStringInputStream);
      bodyStream.setData(body, body.length);
      this.#channel.explicitSetUploadStream(
        bodyStream,
        null,
        -1,
        this.#channel.requestMethod,
        false
      );
    } finally {
      // Make sure to reset the flag once the modification was attempted.
      this.#channel.requestObserversCalled = true;
    }
  }

  /**
   * Set a request header
   *
   * @param {string} name
   *     The header's name.
   * @param {string} value
   *     The header's value.
   * @param {object} options
   * @param {boolean} options.merge
   *     True if the value should be merged with the existing value, false if it
   *     should override it. Defaults to false.
   */
  setRequestHeader(name, value, options) {
    const { merge = false } = options;
    this.#channel.setRequestHeader(name, value, merge);
  }

  /**
   * Update the request's method.
   *
   * @param {string} method
   *     The method to set.
   */
  setRequestMethod(method) {
    // Update the requestObserversCalled flag to allow modifying the request,
    // and reset once done.
    this.#channel.requestObserversCalled = false;

    try {
      this.#channel.requestMethod = method;
    } finally {
      // Make sure to reset the flag once the modification was attempted.
      this.#channel.requestObserversCalled = true;
    }
  }

  /**
   * Allows to bypass the actual network request and immediately respond with
   * the provided nsIReplacedHttpResponse.
   *
   * @param {nsIReplacedHttpResponse} replacedHttpResponse
   *     The replaced response to use.
   */
  setResponseOverride(replacedHttpResponse) {
    this.wrappedChannel.channel
      .QueryInterface(Ci.nsIHttpChannelInternal)
      .setResponseOverride(replacedHttpResponse);

    const rawHeaders = [];
    replacedHttpResponse.visitResponseHeaders({
      visitHeader(name, value) {
        rawHeaders.push(`${name}: ${value}`);
      },
    });

    // Setting an override bypasses the usual codepath for network responses.
    // There will be no notification about receiving a response.
    // However, there will be a notification about the end of the response.
    // Therefore, simulate a addResponseStart here to make sure we handle
    // addResponseContent properly.
    this.#eventRecord.prepareResponseStart({
      channel: this.#channel,
      fromCache: false,
      rawHeaders: rawHeaders.join("\n"),
    });
  }

  /**
   * Return a static version of the class instance.
   * This method is used to prepare the data to be sent with the events for cached resources
   * generated from the content process but need to be sent to the parent.
   */
  toJSON() {
    return {
      headers: this.headers,
      headersSize: this.headersSize,
      method: this.method,
      navigationId: this.navigationId,
      postDataSize: this.postDataSize,
      redirectCount: this.redirectCount,
      requestId: this.requestId,
      serializedURL: this.serializedURL,
      // Since this data is meant to be sent to the parent process
      // it will not be possible to intercept such request.
      supportsInterception: false,
      timings: this.timings,
    };
  }

  /**
   * Convert the provided request timing to a timing relative to the beginning
   * of the request. All timings are numbers representing high definition
   * timestamps.
   *
   * @param {number} timing
   *     High definition timestamp for a request timing relative from the time
   *     origin.
   * @param {number} requestTime
   *     High definition timestamp for the request start time relative from the
   *     time origin.
   *
   * @returns {number}
   *     High definition timestamp for the request timing relative to the start
   *     time of the request, or 0 if the provided timing was 0.
   */
  #convertTimestamp(timing, requestTime) {
    if (timing == 0) {
      return 0;
    }

    return timing - requestTime;
  }

  #getContextId() {
    const id = lazy.NetworkUtils.getChannelBrowsingContextID(this.#channel);
    const browsingContext = BrowsingContext.get(id);
    return lazy.TabManager.getIdForBrowsingContext(browsingContext);
  }

  /**
   * Retrieve the Fetch timings for the NetworkRequest.
   *
   * @returns {object}
   *     Object with keys corresponding to fetch timing names, and their
   *     corresponding values.
   */
  #getFetchTimings() {
    const {
      channelCreationTime,
      redirectStartTime,
      redirectEndTime,
      dispatchFetchEventStartTime,
      cacheReadStartTime,
      domainLookupStartTime,
      domainLookupEndTime,
      connectStartTime,
      connectEndTime,
      secureConnectionStartTime,
      requestStartTime,
      responseStartTime,
      responseEndTime,
    } = this.#timedChannel;

    // fetchStart should be the post-redirect start time, which should be the
    // first non-zero timing from: dispatchFetchEventStart, cacheReadStart and
    // domainLookupStart. See https://www.w3.org/TR/navigation-timing-2/#processing-model
    const fetchStartTime =
      dispatchFetchEventStartTime ||
      cacheReadStartTime ||
      domainLookupStartTime;

    // Bug 1805478: Per spec, the origin time should match Performance API's
    // timeOrigin for the global which initiated the request. This is not
    // available in the parent process, so for now we will use 0.
    const timeOrigin = 0;

    return {
      timeOrigin,
      requestTime: this.#convertTimestamp(channelCreationTime, timeOrigin),
      redirectStart: this.#convertTimestamp(redirectStartTime, timeOrigin),
      redirectEnd: this.#convertTimestamp(redirectEndTime, timeOrigin),
      fetchStart: this.#convertTimestamp(fetchStartTime, timeOrigin),
      dnsStart: this.#convertTimestamp(domainLookupStartTime, timeOrigin),
      dnsEnd: this.#convertTimestamp(domainLookupEndTime, timeOrigin),
      connectStart: this.#convertTimestamp(connectStartTime, timeOrigin),
      connectEnd: this.#convertTimestamp(connectEndTime, timeOrigin),
      tlsStart: this.#convertTimestamp(secureConnectionStartTime, timeOrigin),
      tlsEnd: this.#convertTimestamp(connectEndTime, timeOrigin),
      requestStart: this.#convertTimestamp(requestStartTime, timeOrigin),
      responseStart: this.#convertTimestamp(responseStartTime, timeOrigin),
      responseEnd: this.#convertTimestamp(responseEndTime, timeOrigin),
    };
  }

  /**
   * Retrieve the list of headers for the NetworkRequest.
   *
   * @returns {Array.Array}
   *     Array of (name, value) tuples.
   */
  #getHeadersList() {
    const headers = [];

    if (this.#channel instanceof Ci.nsIHttpChannel) {
      this.#channel.visitRequestHeaders({
        visitHeader(name, value) {
          // The `Proxy-Authorization` header even though it appears on the channel is not
          // actually sent to the server for non CONNECT requests after the HTTP/HTTPS tunnel
          // is setup by the proxy.
          if (name == "Proxy-Authorization") {
            return;
          }
          headers.push([name, value]);
        },
      });
    }

    if (this.#channel instanceof Ci.nsIDataChannel) {
      // Data channels have no request headers.
      return [];
    }

    if (this.#channel instanceof Ci.nsIFileChannel) {
      // File channels have no request headers.
      return [];
    }

    return headers;
  }

  #getNavigationId() {
    if (!this.#channel.isDocument) {
      return null;
    }

    const browsingContext = lazy.TabManager.getBrowsingContextById(
      this.#contextId
    );

    let navigation =
      this.#navigationManager.getNavigationForBrowsingContext(browsingContext);

    // `onBeforeRequestSent` might be too early for the NavigationManager.
    // If there is no ongoing navigation, create one ourselves.
    // TODO: Bug 1835704 to detect navigations earlier and avoid this.
    if (!navigation || navigation.state !== "started") {
      navigation = lazy.notifyNavigationStarted({
        contextDetails: { context: browsingContext },
        url: this.serializedURL,
      });
    }

    return navigation ? navigation.navigationId : null;
  }
}
