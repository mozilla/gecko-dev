Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

const sentCookieVal     = "foo=bar";
const responseBody      = "response body";

XPCOMUtils.defineLazyGetter(this, "baseURL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

const preRedirectPath   = "/528292/pre-redirect";

XPCOMUtils.defineLazyGetter(this, "preRedirectURL", function() {
  return baseURL + preRedirectPath;
});

const postRedirectPath  = "/528292/post-redirect";

XPCOMUtils.defineLazyGetter(this, "postRedirectURL", function() {
  return baseURL + postRedirectPath;
});

var   httpServer        = null;
var   receivedCookieVal = null;

function preRedirectHandler(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 302, "Found");
  response.setHeader("Location", postRedirectURL, false);
  return;
}

function postRedirectHandler(metadata, response)
{
  receivedCookieVal = metadata.getHeader("Cookie");
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write(responseBody, responseBody.length);
}

function inChildProcess() {
  return Cc["@mozilla.org/xre/app-info;1"]
  .getService(Ci.nsIXULRuntime)
  .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function run_test()
{
  // Start the HTTP server.
  httpServer = new HttpServer();
  httpServer.registerPathHandler(preRedirectPath, preRedirectHandler);
  httpServer.registerPathHandler(postRedirectPath, postRedirectHandler);
  httpServer.start(-1);

  if (!inChildProcess()) {
    // Disable third-party cookies in general.
    Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch).
      setIntPref("network.cookie.cookieBehavior", 1);
  }

  var ioService = Cc["@mozilla.org/network/io-service;1"].
                  getService(Ci.nsIIOService);

  // Set up a channel with forceAllowThirdPartyCookie set to true.  We'll use
  // the channel both to set a cookie (since nsICookieService::setCookieString
  // requires such a channel in order to successfully set a cookie) and then
  // to load the pre-redirect URI.
  var chan = ioService.newChannel2(preRedirectURL,
                                   "",
                                   null,
                                   null,      // aLoadingNode
                                   Services.scriptSecurityManager.getSystemPrincipal(),
                                   null,      // aTriggeringPrincipal
                                   Ci.nsILoadInfo.SEC_NORMAL,
                                   Ci.nsIContentPolicy.TYPE_OTHER).
             QueryInterface(Ci.nsIHttpChannel).
             QueryInterface(Ci.nsIHttpChannelInternal);
  chan.forceAllowThirdPartyCookie = true;

  // Set a cookie on one of the URIs.  It doesn't matter which one, since
  // they're both from the same host, which is enough for the cookie service
  // to send the cookie with both requests.
  var postRedirectURI = ioService.newURI(postRedirectURL, "", null);
  Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService).
    setCookieString(postRedirectURI, null, sentCookieVal, chan);

  // Load the pre-redirect URI.
  chan.asyncOpen(new ChannelListener(finish_test, null), null);
  do_test_pending();
}

function finish_test(event)
{
  do_check_eq(receivedCookieVal, sentCookieVal);
  httpServer.stop(do_test_finished);
}
