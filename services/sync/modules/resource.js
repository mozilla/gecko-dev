/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
  "AsyncResource",
  "Resource"
];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/util.js");

const DEFAULT_LOAD_FLAGS =
  // Always validate the cache:
  Ci.nsIRequest.LOAD_BYPASS_CACHE |
  Ci.nsIRequest.INHIBIT_CACHING |
  // Don't send user cookies over the wire (Bug 644734).
  Ci.nsIRequest.LOAD_ANONYMOUS;

/*
 * AsyncResource represents a remote network resource, identified by a URI.
 * Create an instance like so:
 *
 *   let resource = new AsyncResource("http://foobar.com/path/to/resource");
 *
 * The 'resource' object has the following methods to issue HTTP requests
 * of the corresponding HTTP methods:
 *
 *   get(callback)
 *   put(data, callback)
 *   post(data, callback)
 *   delete(callback)
 *
 * 'callback' is a function with the following signature:
 *
 *   function callback(error, result) {...}
 *
 * 'error' will be null on successful requests. Likewise, result will not be
 * passed (=undefined) when an error occurs. Note that this is independent of
 * the status of the HTTP response.
 */
this.AsyncResource = function AsyncResource(uri) {
  this._log = Log.repository.getLogger(this._logName);
  this._log.level =
    Log.Level[Svc.Prefs.get("log.logger.network.resources")];
  this.uri = uri;
  this._headers = {};
  this._onComplete = Utils.bind2(this, this._onComplete);
}
AsyncResource.prototype = {
  _logName: "Sync.AsyncResource",

  // ** {{{ AsyncResource.serverTime }}} **
  //
  // Caches the latest server timestamp (X-Weave-Timestamp header).
  serverTime: null,

  /**
   * Callback to be invoked at request time to add authentication details.
   *
   * By default, a global authenticator is provided. If this is set, it will
   * be used instead of the global one.
   */
  authenticator: null,

  // The string to use as the base User-Agent in Sync requests.
  // These strings will look something like
  //
  //   Firefox/4.0 FxSync/1.8.0.20100101.mobile
  //
  // or
  //
  //   Firefox Aurora/5.0a1 FxSync/1.9.0.20110409.desktop
  //
  _userAgent:
    Services.appinfo.name + "/" + Services.appinfo.version +  // Product.
    " FxSync/" + WEAVE_VERSION + "." +                        // Sync.
    Services.appinfo.appBuildID + ".",                        // Build.

  // Wait 5 minutes before killing a request.
  ABORT_TIMEOUT: 300000,

  // ** {{{ AsyncResource.headers }}} **
  //
  // Headers to be included when making a request for the resource.
  // Note: Header names should be all lower case, there's no explicit
  // check for duplicates due to case!
  get headers() {
    return this._headers;
  },
  set headers(value) {
    this._headers = value;
  },
  setHeader: function Res_setHeader(header, value) {
    this._headers[header.toLowerCase()] = value;
  },
  get headerNames() {
    return Object.keys(this.headers);
  },

  // ** {{{ AsyncResource.uri }}} **
  //
  // URI representing this resource.
  get uri() {
    return this._uri;
  },
  set uri(value) {
    if (typeof value == 'string')
      this._uri = CommonUtils.makeURI(value);
    else
      this._uri = value;
  },

  // ** {{{ AsyncResource.spec }}} **
  //
  // Get the string representation of the URI.
  get spec() {
    if (this._uri)
      return this._uri.spec;
    return null;
  },

  // ** {{{ AsyncResource.data }}} **
  //
  // Get and set the data encapulated in the resource.
  _data: null,
  get data() this._data,
  set data(value) {
    this._data = value;
  },

  // ** {{{ AsyncResource._createRequest }}} **
  //
  // This method returns a new IO Channel for requests to be made
  // through. It is never called directly, only {{{_doRequest}}} uses it
  // to obtain a request channel.
  //
  _createRequest: function Res__createRequest(method) {
    let channel = Services.io.newChannel2(this.spec,
                                          null,
                                          null,
                                          null,      // aLoadingNode
                                          Services.scriptSecurityManager.getSystemPrincipal(),
                                          null,      // aTriggeringPrincipal
                                          Ci.nsILoadInfo.SEC_NORMAL,
                                          Ci.nsIContentPolicy.TYPE_OTHER)
                          .QueryInterface(Ci.nsIRequest)
                          .QueryInterface(Ci.nsIHttpChannel);

    channel.loadFlags |= DEFAULT_LOAD_FLAGS;

    // Setup a callback to handle channel notifications.
    let listener = new ChannelNotificationListener(this.headerNames);
    channel.notificationCallbacks = listener;

    // Compose a UA string fragment from the various available identifiers.
    if (Svc.Prefs.get("sendVersionInfo", true)) {
      let ua = this._userAgent + Svc.Prefs.get("client.type", "desktop");
      channel.setRequestHeader("user-agent", ua, false);
    }

    let headers = this.headers;

    if (this.authenticator) {
      let result = this.authenticator(this, method);
      if (result && result.headers) {
        for (let [k, v] in Iterator(result.headers)) {
          headers[k.toLowerCase()] = v;
        }
      }
    } else {
      this._log.debug("No authenticator found.");
    }

    for (let [key, value] in Iterator(headers)) {
      if (key == 'authorization')
        this._log.trace("HTTP Header " + key + ": ***** (suppressed)");
      else
        this._log.trace("HTTP Header " + key + ": " + headers[key]);
      channel.setRequestHeader(key, headers[key], false);
    }
    return channel;
  },

  _onProgress: function Res__onProgress(channel) {},

  _doRequest: function _doRequest(action, data, callback) {
    this._log.trace("In _doRequest.");
    this._callback = callback;
    let channel = this._createRequest(action);

    if ("undefined" != typeof(data))
      this._data = data;

    // PUT and POST are treated differently because they have payload data.
    if ("PUT" == action || "POST" == action) {
      // Convert non-string bodies into JSON
      if (this._data.constructor.toString() != String)
        this._data = JSON.stringify(this._data);

      this._log.debug(action + " Length: " + this._data.length);
      this._log.trace(action + " Body: " + this._data);

      let type = ('content-type' in this._headers) ?
        this._headers['content-type'] : 'text/plain';

      let stream = Cc["@mozilla.org/io/string-input-stream;1"].
        createInstance(Ci.nsIStringInputStream);
      stream.setData(this._data, this._data.length);

      channel.QueryInterface(Ci.nsIUploadChannel);
      channel.setUploadStream(stream, type, this._data.length);
    }

    // Setup a channel listener so that the actual network operation
    // is performed asynchronously.
    let listener = new ChannelListener(this._onComplete, this._onProgress,
                                       this._log, this.ABORT_TIMEOUT);
    channel.requestMethod = action;
    try {
      channel.asyncOpen(listener, null);
    } catch (ex) {
      // asyncOpen can throw in a bunch of cases -- e.g., a forbidden port.
      this._log.warn("Caught an error in asyncOpen: " + CommonUtils.exceptionStr(ex));
      CommonUtils.nextTick(callback.bind(this, ex));
    }
  },

  _onComplete: function _onComplete(error, data, channel) {
    this._log.trace("In _onComplete. Error is " + error + ".");

    if (error) {
      this._callback(error);
      return;
    }

    this._data = data;
    let action = channel.requestMethod;

    this._log.trace("Channel: " + channel);
    this._log.trace("Action: "  + action);

    // Process status and success first. This way a problem with headers
    // doesn't fail to include accurate status information.
    let status = 0;
    let success = false;

    try {
      status  = channel.responseStatus;
      success = channel.requestSucceeded;    // HTTP status.

      this._log.trace("Status: " + status);
      this._log.trace("Success: " + success);

      // Log the status of the request.
      let mesg = [action, success ? "success" : "fail", status,
                  channel.URI.spec].join(" ");
      this._log.debug("mesg: " + mesg);

      if (mesg.length > 200)
        mesg = mesg.substr(0, 200) + "…";
      this._log.debug(mesg);

      // Additionally give the full response body when Trace logging.
      if (this._log.level <= Log.Level.Trace)
        this._log.trace(action + " body: " + data);

    } catch(ex) {
      // Got a response, but an exception occurred during processing.
      // This shouldn't occur.
      this._log.warn("Caught unexpected exception " + CommonUtils.exceptionStr(ex) +
                     " in _onComplete.");
      this._log.debug(CommonUtils.stackTrace(ex));
    }

    // Process headers. They can be empty, or the call can otherwise fail, so
    // put this in its own try block.
    let headers = {};
    try {
      this._log.trace("Processing response headers.");

      // Read out the response headers if available.
      channel.visitResponseHeaders({
        visitHeader: function visitHeader(header, value) {
          headers[header.toLowerCase()] = value;
        }
      });

      // This is a server-side safety valve to allow slowing down
      // clients without hurting performance.
      if (headers["x-weave-backoff"]) {
        let backoff = headers["x-weave-backoff"];
        this._log.debug("Got X-Weave-Backoff: " + backoff);
        Observers.notify("weave:service:backoff:interval",
                         parseInt(backoff, 10));
      }

      if (success && headers["x-weave-quota-remaining"]) {
        Observers.notify("weave:service:quota:remaining",
                         parseInt(headers["x-weave-quota-remaining"], 10));
      }

      let contentLength = headers["content-length"];
      if (success && contentLength && data &&
          contentLength != data.length) {
        this._log.warn("The response body's length of: " + data.length +
                       " doesn't match the header's content-length of: " +
                       contentLength + ".");
      }
    } catch (ex) {
      this._log.debug("Caught exception " + CommonUtils.exceptionStr(ex) +
                      " visiting headers in _onComplete.");
      this._log.debug(CommonUtils.stackTrace(ex));
    }

    let ret     = new String(data);
    ret.status  = status;
    ret.success = success;
    ret.headers = headers;

    // Make a lazy getter to convert the json response into an object.
    // Note that this can cause a parse error to be thrown far away from the
    // actual fetch, so be warned!
    XPCOMUtils.defineLazyGetter(ret, "obj", function() {
      try {
        return JSON.parse(ret);
      } catch (ex) {
        this._log.warn("Got exception parsing response body: \"" + CommonUtils.exceptionStr(ex));
        // Stringify to avoid possibly printing non-printable characters.
        this._log.debug("Parse fail: Response body starts: \"" +
                        JSON.stringify((ret + "").slice(0, 100)) +
                        "\".");
        throw ex;
      }
    }.bind(this));

    this._callback(null, ret);
  },

  get: function get(callback) {
    this._doRequest("GET", undefined, callback);
  },

  put: function put(data, callback) {
    if (typeof data == "function")
      [data, callback] = [undefined, data];
    this._doRequest("PUT", data, callback);
  },

  post: function post(data, callback) {
    if (typeof data == "function")
      [data, callback] = [undefined, data];
    this._doRequest("POST", data, callback);
  },

  delete: function delete_(callback) {
    this._doRequest("DELETE", undefined, callback);
  }
};


/*
 * Represent a remote network resource, identified by a URI, with a
 * synchronous API.
 *
 * 'Resource' is not recommended for new code. Use the asynchronous API of
 * 'AsyncResource' instead.
 */
this.Resource = function Resource(uri) {
  AsyncResource.call(this, uri);
}
Resource.prototype = {

  __proto__: AsyncResource.prototype,

  _logName: "Sync.Resource",

  // ** {{{ Resource._request }}} **
  //
  // Perform a particular HTTP request on the resource. This method
  // is never called directly, but is used by the high-level
  // {{{get}}}, {{{put}}}, {{{post}}} and {{delete}} methods.
  _request: function Res__request(action, data) {
    let cb = Async.makeSyncCallback();
    function callback(error, ret) {
      if (error)
        cb.throw(error);
      else
        cb(ret);
    }

    // The channel listener might get a failure code
    try {
      this._doRequest(action, data, callback);
      return Async.waitForSyncCallback(cb);
    } catch(ex) {
      // Combine the channel stack with this request stack.  Need to create
      // a new error object for that.
      let error = Error(ex.message);
      error.result = ex.result;
      let chanStack = [];
      if (ex.stack)
        chanStack = ex.stack.trim().split(/\n/).slice(1);
      let requestStack = error.stack.split(/\n/).slice(1);

      // Strip out the args for the last 2 frames because they're usually HUGE!
      for (let i = 0; i <= 1; i++)
        requestStack[i] = requestStack[i].replace(/\(".*"\)@/, "(...)@");

      error.stack = chanStack.concat(requestStack).join("\n");
      throw error;
    }
  },

  // ** {{{ Resource.get }}} **
  //
  // Perform an asynchronous HTTP GET for this resource.
  get: function Res_get() {
    return this._request("GET");
  },

  // ** {{{ Resource.put }}} **
  //
  // Perform a HTTP PUT for this resource.
  put: function Res_put(data) {
    return this._request("PUT", data);
  },

  // ** {{{ Resource.post }}} **
  //
  // Perform a HTTP POST for this resource.
  post: function Res_post(data) {
    return this._request("POST", data);
  },

  // ** {{{ Resource.delete }}} **
  //
  // Perform a HTTP DELETE for this resource.
  delete: function Res_delete() {
    return this._request("DELETE");
  }
};

// = ChannelListener =
//
// This object implements the {{{nsIStreamListener}}} interface
// and is called as the network operation proceeds.
function ChannelListener(onComplete, onProgress, logger, timeout) {
  this._onComplete = onComplete;
  this._onProgress = onProgress;
  this._log = logger;
  this._timeout = timeout;
  this.delayAbort();
}
ChannelListener.prototype = {

  onStartRequest: function Channel_onStartRequest(channel) {
    this._log.trace("onStartRequest called for channel " + channel + ".");

    try {
      channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      this._log.error("Unexpected error: channel is not a nsIHttpChannel!");
      channel.cancel(Cr.NS_BINDING_ABORTED);
      return;
    }

    // Save the latest server timestamp when possible.
    try {
      AsyncResource.serverTime = channel.getResponseHeader("X-Weave-Timestamp") - 0;
    }
    catch(ex) {}

    this._log.trace("onStartRequest: " + channel.requestMethod + " " +
                    channel.URI.spec);
    this._data = '';
    this.delayAbort();
  },

  onStopRequest: function Channel_onStopRequest(channel, context, status) {
    // Clear the abort timer now that the channel is done.
    this.abortTimer.clear();

    if (!this._onComplete) {
      this._log.error("Unexpected error: _onComplete not defined in onStopRequest.");
      this._onProgress = null;
      return;
    }

    try {
      channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      this._log.error("Unexpected error: channel is not a nsIHttpChannel!");

      this._onComplete(ex, this._data, channel);
      this._onComplete = this._onProgress = null;
      return;
    }

    let statusSuccess = Components.isSuccessCode(status);
    let uri = channel && channel.URI && channel.URI.spec || "<unknown>";
    this._log.trace("Channel for " + channel.requestMethod + " " + uri + ": " +
                    "isSuccessCode(" + status + ")? " + statusSuccess);

    if (this._data == '') {
      this._data = null;
    }

    // Pass back the failure code and stop execution. Use Components.Exception()
    // instead of Error() so the exception is QI-able and can be passed across
    // XPCOM borders while preserving the status code.
    if (!statusSuccess) {
      let message = Components.Exception("", status).name;
      let error   = Components.Exception(message, status);

      this._onComplete(error, undefined, channel);
      this._onComplete = this._onProgress = null;
      return;
    }

    this._log.trace("Channel: flags = " + channel.loadFlags +
                    ", URI = " + uri +
                    ", HTTP success? " + channel.requestSucceeded);
    this._onComplete(null, this._data, channel);
    this._onComplete = this._onProgress = null;
  },

  onDataAvailable: function Channel_onDataAvail(req, cb, stream, off, count) {
    let siStream;
    try {
      siStream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(Ci.nsIScriptableInputStream);
      siStream.init(stream);
    } catch (ex) {
      this._log.warn("Exception creating nsIScriptableInputStream." + CommonUtils.exceptionStr(ex));
      this._log.debug("Parameters: " + req.URI.spec + ", " + stream + ", " + off + ", " + count);
      // Cannot proceed, so rethrow and allow the channel to cancel itself.
      throw ex;
    }

    try {
      this._data += siStream.read(count);
    } catch (ex) {
      this._log.warn("Exception thrown reading " + count + " bytes from " + siStream + ".");
      throw ex;
    }

    try {
      this._onProgress();
    } catch (ex) {
      this._log.warn("Got exception calling onProgress handler during fetch of "
                     + req.URI.spec);
      this._log.debug(CommonUtils.exceptionStr(ex));
      this._log.trace("Rethrowing; expect a failure code from the HTTP channel.");
      throw ex;
    }

    this.delayAbort();
  },

  /**
   * Create or push back the abort timer that kills this request.
   */
  delayAbort: function delayAbort() {
    try {
      CommonUtils.namedTimer(this.abortRequest, this._timeout, this, "abortTimer");
    } catch (ex) {
      this._log.warn("Got exception extending abort timer: " + CommonUtils.exceptionStr(ex));
    }
  },

  abortRequest: function abortRequest() {
    // Ignore any callbacks if we happen to get any now
    this.onStopRequest = function() {};
    let error = Components.Exception("Aborting due to channel inactivity.",
                                     Cr.NS_ERROR_NET_TIMEOUT);
    if (!this._onComplete) {
      this._log.error("Unexpected error: _onComplete not defined in " +
                      "abortRequest.");
      return;
    }
    this._onComplete(error);
  }
};

/**
 * This class handles channel notification events.
 *
 * An instance of this class is bound to each created channel.
 *
 * Optionally pass an array of header names. Each header named
 * in this array will be copied between the channels in the
 * event of a redirect.
 */
function ChannelNotificationListener(headersToCopy) {
  this._headersToCopy = headersToCopy;

  this._log = Log.repository.getLogger(this._logName);
  this._log.level = Log.Level[Svc.Prefs.get("log.logger.network.resources")];
}
ChannelNotificationListener.prototype = {
  _logName: "Sync.Resource",

  getInterface: function(aIID) {
    return this.QueryInterface(aIID);
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIBadCertListener2) ||
        aIID.equals(Ci.nsIInterfaceRequestor) ||
        aIID.equals(Ci.nsISupports) ||
        aIID.equals(Ci.nsIChannelEventSink))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  notifyCertProblem: function certProblem(socketInfo, sslStatus, targetHost) {
    let log = Log.repository.getLogger("Sync.CertListener");
    log.warn("Invalid HTTPS certificate encountered!");

    // This suppresses the UI warning only. The request is still cancelled.
    return true;
  },

  asyncOnChannelRedirect:
    function asyncOnChannelRedirect(oldChannel, newChannel, flags, callback) {

    let oldSpec = (oldChannel && oldChannel.URI) ? oldChannel.URI.spec : "<undefined>";
    let newSpec = (newChannel && newChannel.URI) ? newChannel.URI.spec : "<undefined>";
    this._log.debug("Channel redirect: " + oldSpec + ", " + newSpec + ", " + flags);

    this._log.debug("Ensuring load flags are set.");
    newChannel.loadFlags |= DEFAULT_LOAD_FLAGS;

    // For internal redirects, copy the headers that our caller set.
    try {
      if ((flags & Ci.nsIChannelEventSink.REDIRECT_INTERNAL) &&
          newChannel.URI.equals(oldChannel.URI)) {
        this._log.debug("Copying headers for safe internal redirect.");

        // QI the channel so we can set headers on it.
        try {
          newChannel.QueryInterface(Ci.nsIHttpChannel);
        } catch (ex) {
          this._log.error("Unexpected error: channel is not a nsIHttpChannel!");
          throw ex;
        }

        for (let header of this._headersToCopy) {
          let value = oldChannel.getRequestHeader(header);
          if (value) {
            let printed = (header == "authorization") ? "****" : value;
            this._log.debug("Header: " + header + " = " + printed);
            newChannel.setRequestHeader(header, value, false);
          } else {
            this._log.warn("No value for header " + header);
          }
        }
      }
    } catch (ex) {
      this._log.error("Error copying headers: " + CommonUtils.exceptionStr(ex));
    }

    // We let all redirects proceed.
    try {
      callback.onRedirectVerifyCallback(Cr.NS_OK);
    } catch (ex) {
      this._log.error("onRedirectVerifyCallback threw!" + CommonUtils.exceptionStr(ex));
    }
  }
};
