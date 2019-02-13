Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/MockRegistrar.jsm");

var httpserv = null;

const CID = Components.ID("{5645d2c1-d6d8-4091-b117-fe7ee4027db7}");
XPCOMUtils.defineLazyGetter(this, "systemSettings", function() {
  return {
    QueryInterface: function (iid) {
      if (iid.equals(Components.interfaces.nsISupports) ||
          iid.equals(Components.interfaces.nsISystemProxySettings))
        return this;
      throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    mainThreadOnly: true,
    PACURI: "http://localhost:" + httpserv.identity.primaryPort + "/redirect",
    getProxyForURI: function(aURI) {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
  };
});

function checkValue(request, data, ctx) {
  do_check_true(called);
  do_check_eq("ok", data);
  httpserv.stop(do_test_finished);
}

function makeChan(url) {
  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  var chan = ios.newChannel2(url,
                             null,
                             null,
                             null,      // aLoadingNode
                             Services.scriptSecurityManager.getSystemPrincipal(),
                             null,      // aTriggeringPrincipal
                             Ci.nsILoadInfo.SEC_NORMAL,
                             Ci.nsIContentPolicy.TYPE_OTHER)
                .QueryInterface(Components.interfaces.nsIHttpChannel);

  return chan;
}

function run_test() {
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/redirect", redirect);
  httpserv.registerPathHandler("/pac", pac);
  httpserv.registerPathHandler("/target", target);
  httpserv.start(-1);

  MockRegistrar.register("@mozilla.org/system-proxy-settings;1",
                         systemSettings);

  // Ensure we're using system-properties
  const prefs = Cc["@mozilla.org/preferences-service;1"]
                   .getService(Ci.nsIPrefBranch);
  prefs.setIntPref(
    "network.proxy.type",
    Components.interfaces.nsIProtocolProxyService.PROXYCONFIG_SYSTEM);

  // clear cache
  evict_cache_entries();

  var chan = makeChan("http://localhost:" + httpserv.identity.primaryPort +
                      "/target");
  chan.asyncOpen(new ChannelListener(checkValue, null), null);

  do_test_pending();
}

var called = false, failed = false;
function redirect(metadata, response) {
  // If called second time, just return the PAC but set failed-flag
  if (called) {
      failed = true;
      return pac(metadata, response);
   }
  
  called = true;
  response.setStatusLine(metadata.httpVersion, 302, "Found");
  response.setHeader("Location", "/pac", false);
  var body = "Moved\n";
  response.bodyOutputStream.write(body, body.length);
}

function pac(metadata, response) {
  var PAC = 'function FindProxyForURL(url, host) { return "DIRECT"; }';
  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "application/x-ns-proxy-autoconfig", false);
  response.bodyOutputStream.write(PAC, PAC.length);
}

function target(metadata, response) {
  var retval = "ok";
  if (failed) retval = "failed";

  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(retval, retval.length);
}
