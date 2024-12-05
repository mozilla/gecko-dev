/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  NetworkRequest: "chrome://remote/content/shared/NetworkRequest.sys.mjs",
  NetworkResponse: "chrome://remote/content/shared/NetworkResponse.sys.mjs",
  NetworkUtils:
    "resource://devtools/shared/network-observer/NetworkUtils.sys.mjs",

  Log: "chrome://remote/content/shared/Log.sys.mjs",
  truncate: "chrome://remote/content/shared/Format.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

/**
 * The NetworkEventRecord implements the interface expected from network event
 * owners for consumers of the DevTools NetworkObserver.
 *
 * The NetworkEventRecord emits the before-request-sent event on behalf of the
 * NetworkListener instance which created it.
 */
export class NetworkEventRecord {
  #decodedBodySizeMap;
  #fromCache;
  #networkEventsMap;
  #networkListener;
  #request;
  #response;
  #responseStartOverride;
  #wrappedChannel;

  /**
   *
   * @param {object} networkEvent
   *     The initial network event information (see createNetworkEvent() in
   *     NetworkUtils.sys.mjs).
   * @param {nsIChannel} channel
   *     The nsIChannel behind this network event.
   * @param {NetworkListener} networkListener
   *     The NetworkListener which created this NetworkEventRecord.
   * @param {NetworkDecodedBodySizeMap} decodedBodySizeMap
   *     Map from channelId to decoded body sizes. This information is read
   *     from all processes and aggregated in the parent process.
   * @param {NavigationManager} navigationManager
   *     The NavigationManager which belongs to the same session as this
   *     NetworkEventRecord.
   * @param {Map<string, NetworkEventRecord>} networkEventsMap
   *     The map between request id and NetworkEventRecord instance to complete
   *     the previous event in case of redirect.
   */
  constructor(
    networkEvent,
    channel,
    networkListener,
    decodedBodySizeMap,
    navigationManager,
    networkEventsMap
  ) {
    this.#request = new lazy.NetworkRequest(channel, {
      eventRecord: this,
      navigationManager,
      rawHeaders: networkEvent.rawHeaders,
    });
    this.#response = null;

    if (channel instanceof Ci.nsIChannel) {
      this.#wrappedChannel = ChannelWrapper.get(channel);
      this.#wrappedChannel.addEventListener("error", this.#onChannelCompleted);
      this.#wrappedChannel.addEventListener("stop", this.#onChannelCompleted);
    }

    this.#fromCache = networkEvent.fromCache;

    this.#decodedBodySizeMap = decodedBodySizeMap;
    this.#networkListener = networkListener;
    this.#networkEventsMap = networkEventsMap;

    if (this.#networkEventsMap.has(this.#requestId)) {
      const previousEvent = this.#networkEventsMap.get(this.#requestId);
      if (this.redirectCount != previousEvent.redirectCount) {
        // If redirect count is set, this is a redirect from the previous request.
        // notifyRedirect will complete the previous request.
        previousEvent.notifyRedirect();
      } else {
        // Otherwise if there is no redirect count or if it is identical to the
        // previously detected request, this is an authentication attempt.
        previousEvent.notifyAuthenticationAttempt();
      }
    }

    this.#networkEventsMap.set(this.#requestId, this);

    // NetworkObserver creates a network event when request headers have been
    // parsed.
    // According to the BiDi spec, we should emit beforeRequestSent when adding
    // request headers, see https://whatpr.org/fetch/1540.html#http-network-or-cache-fetch
    // step 8.17
    // Bug 1802181: switch the NetworkObserver to an event-based API.
    this.#emitBeforeRequestSent();

    // If the request is already blocked, we will not receive further updates,
    // emit a network.fetchError event immediately.
    if (networkEvent.blockedReason) {
      this.#emitFetchError();
    }
  }

  get #requestId() {
    return this.#request.requestId;
  }

  get redirectCount() {
    return this.#request.redirectCount;
  }

  /**
   * Add network request cache details.
   *
   * Required API for a NetworkObserver event owner.
   *
   * @param {object} options
   * @param {boolean} options.fromCache
   */
  addCacheDetails(options) {
    const { fromCache } = options;
    this.#fromCache = fromCache;
  }

  /**
   * Add network request raw headers.
   *
   * Required API for a NetworkObserver event owner.
   *
   * @param {object} options
   * @param {string} options.rawHeaders
   */
  addRawHeaders(options) {
    const { rawHeaders } = options;
    this.#request.addRawHeaders(rawHeaders);
  }

  /**
   * Add network request POST data.
   *
   * Required API for a NetworkObserver event owner.
   */
  addRequestPostData() {}

  /**
   * Add the initial network response information.
   *
   * Required API for a NetworkObserver event owner.
   *
   * @param {object} options
   * @param {nsIChannel} options.channel
   *     The channel.
   * @param {boolean} options.fromCache
   * @param {boolean} options.fromServiceWorker
   * @param {string} options.rawHeaders
   */
  addResponseStart(options) {
    const { channel, fromCache, fromServiceWorker, rawHeaders } = options;
    this.#response = new lazy.NetworkResponse(channel, {
      fromCache: this.#fromCache || !!fromCache,
      fromServiceWorker,
      rawHeaders,
    });

    // This should be triggered when all headers have been received, matching
    // the WebDriverBiDi response started trigger in `4.6. HTTP-network fetch`
    // from the fetch specification, based on the PR visible at
    // https://github.com/whatwg/fetch/pull/1540
    this.#emitResponseStarted();
  }

  /**
   * Add connection security information.
   *
   * Required API for a NetworkObserver event owner.
   *
   * Not used for RemoteAgent.
   */
  addSecurityInfo() {}

  /**
   * Add network event timings.
   *
   * Required API for a NetworkObserver event owner.
   *
   * Not used for RemoteAgent.
   */
  addEventTimings() {}

  /**
   * Add response cache entry.
   *
   * Required API for a NetworkObserver event owner.
   *
   * Not used for RemoteAgent.
   */
  addResponseCache() {}

  /**
   * Add response content.
   *
   * Required API for a NetworkObserver event owner.
   *
   * @param {object} responseContent
   *     An object which represents the response content.
   * @param {object} responseInfo
   *     Additional meta data about the response.
   */
  addResponseContent(responseContent, responseInfo) {
    if (
      // Ignore already completed requests.
      this.#request.alreadyCompleted ||
      // Ignore HTTP channels which are not service worker requests, they will
      // be handled via "error" and "stop" events, see #onChannelCompleted.
      (this.#request.isHttpChannel && !this.#response?.fromServiceWorker)
    ) {
      return;
    }

    const sizes = {
      decodedBodySize: responseContent.decodedBodySize,
      encodedBodySize: responseContent.bodySize,
      totalTransmittedSize: responseContent.transferredSize,
    };
    this.#handleRequestEnd(responseInfo.blockedReason, sizes);
  }

  /**
   * Add server timings.
   *
   * Required API for a NetworkObserver event owner.
   *
   * Not used for RemoteAgent.
   */
  addServerTimings() {}

  /**
   * Add service worker timings.
   *
   * Required API for a NetworkObserver event owner.
   *
   * Not used for RemoteAgent.
   */
  addServiceWorkerTimings() {}

  /**
   * Complete response in case of an authentication attempt.
   *
   * This method is required to be called on the previous event.
   */
  notifyAuthenticationAttempt() {
    // TODO: Bug 1899604, behavior might change based on spec issue
    // https://github.com/w3c/webdriver-bidi/issues/722

    // For now, in case of authentication attempts, we mark the current event as
    // completed and skip its responseCompleted event.
    // This way, only the last successful/failed authentication attempt will
    // emit a response completed event.
    this.#markRequestComplete();
  }

  /**
   * Complete response in case of redirect.
   *
   * This method is required to be called on the previous event.
   */
  notifyRedirect() {
    this.#emitResponseCompleted();
    this.#markRequestComplete();
  }

  onAuthPrompt(authDetails, authCallbacks) {
    this.#emitAuthRequired(authCallbacks);
  }

  prepareResponseStart(options) {
    this.#responseStartOverride = options;
  }

  #emitAuthRequired(authCallbacks) {
    this.#networkListener.emit("auth-required", {
      authCallbacks,
      request: this.#request,
      response: this.#response,
    });
  }

  #emitBeforeRequestSent() {
    this.#networkListener.emit("before-request-sent", {
      request: this.#request,
    });
  }

  #emitFetchError() {
    this.#networkListener.emit("fetch-error", {
      request: this.#request,
    });
  }

  #emitResponseCompleted() {
    this.#networkListener.emit("response-completed", {
      request: this.#request,
      response: this.#response,
    });
  }

  #emitResponseStarted() {
    this.#networkListener.emit("response-started", {
      request: this.#request,
      response: this.#response,
    });
  }

  #handleRequestEnd(blockedReason, sizes) {
    if (this.#responseStartOverride) {
      this.addResponseStart(this.#responseStartOverride);
    }

    if (blockedReason) {
      this.#emitFetchError();
    } else {
      // In the meantime, if the request was already completed, bail out here.
      if (this.#request.alreadyCompleted) {
        return;
      }

      if (!this.#response) {
        lazy.logger.warn(
          lazy.truncate`Missing response info, network.responseCompleted will be skipped for URL: ${this.#request.serializedURL}`
        );
      } else {
        this.#response.setResponseSizes(sizes);
        this.#emitResponseCompleted();
      }
    }

    this.#markRequestComplete();
  }

  #markRequestComplete() {
    this.#request.alreadyCompleted = true;
    this.#networkEventsMap.delete(this.#requestId);
    this.#decodedBodySizeMap.delete(this.#request.channel.channelId);

    if (this.#wrappedChannel) {
      this.#wrappedChannel.removeEventListener(
        "error",
        this.#onChannelCompleted
      );
      this.#wrappedChannel.removeEventListener(
        "stop",
        this.#onChannelCompleted
      );
    }
  }

  #onChannelCompleted = async () => {
    if (this.#request.alreadyCompleted) {
      return;
    }

    const { blockedReason } = lazy.NetworkUtils.getBlockedReason(
      this.#request.channel,
      this.#response ? this.#response.fromCache : false
    );

    // TODO: Figure out a good default value for the decoded body size for non
    // http channels.
    // Blocked channels will emit a fetchError event which does not contain
    // sizes.
    const sizes = {};
    if (this.#request.isHttpChannel && !blockedReason) {
      sizes.decodedBodySize = await this.#decodedBodySizeMap.getDecodedBodySize(
        this.#request.channel.channelId
      );
      sizes.encodedBodySize = this.#request.channel.encodedBodySize;
      sizes.totalTransmittedSize = this.#request.channel.transferSize;
    }

    this.#handleRequestEnd(blockedReason, sizes);
  };
}
