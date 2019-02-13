"use strict";
// https://bugzilla.mozilla.org/show_bug.cgi?id=760955

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

var httpServer = null;
const testFileName = "test_nsHttpChannel_CacheForOfflineUse-no-store";
const cacheClientID = testFileName + "|fake-group-id";
const basePath = "/" + testFileName + "/";

XPCOMUtils.defineLazyGetter(this, "baseURI", function() {
  return "http://localhost:" + httpServer.identity.primaryPort + basePath;
});

const normalEntry = "normal";
const noStoreEntry = "no-store";

var cacheUpdateObserver = null;
var appCache = null;

function make_channel_for_offline_use(url, callback, ctx) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var chan = ios.newChannel2(url,
                             "",
                             null,
                             null,      // aLoadingNode
                             Services.scriptSecurityManager.getSystemPrincipal(),
                             null,      // aTriggeringPrincipal
                             Ci.nsILoadInfo.SEC_NORMAL,
                             Ci.nsIContentPolicy.TYPE_OTHER);
  
  var cacheService = Components.classes["@mozilla.org/network/application-cache-service;1"].
                     getService(Components.interfaces.nsIApplicationCacheService);
  appCache = cacheService.getApplicationCache(cacheClientID);
  
  var appCacheChan = chan.QueryInterface(Ci.nsIApplicationCacheChannel);
  appCacheChan.applicationCacheForWrite = appCache;
  return chan;
}

function make_uri(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newURI(url, null, null);
}

const responseBody = "response body";

// A HTTP channel for updating the offline cache should normally succeed.
function normalHandler(metadata, response)
{
  do_print("normalHandler");
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write(responseBody, responseBody.length);
}
function checkNormal(request, buffer)
{
  do_check_eq(buffer, responseBody);
  asyncCheckCacheEntryPresence(baseURI + normalEntry, "appcache", true, run_next_test, appCache);
}
add_test(function test_normal() {
  var chan = make_channel_for_offline_use(baseURI + normalEntry);
  chan.asyncOpen(new ChannelListener(checkNormal, chan), null);
});

// An HTTP channel for updating the offline cache should fail when it gets a
// response with Cache-Control: no-store.
function noStoreHandler(metadata, response)
{
  do_print("noStoreHandler");
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-Control", "no-store");
  response.bodyOutputStream.write(responseBody, responseBody.length);
}
function checkNoStore(request, buffer)
{
  do_check_eq(buffer, "");
  asyncCheckCacheEntryPresence(baseURI + noStoreEntry, "appcache", false, run_next_test, appCache);
}
add_test(function test_noStore() {
  var chan = make_channel_for_offline_use(baseURI + noStoreEntry);
  // The no-store should cause the channel to fail to load.
  chan.asyncOpen(new ChannelListener(checkNoStore, chan, CL_EXPECT_FAILURE),
                 null);
});

function run_test()
{
  do_get_profile();

  httpServer = new HttpServer();
  httpServer.registerPathHandler(basePath + normalEntry, normalHandler);
  httpServer.registerPathHandler(basePath + noStoreEntry, noStoreHandler);
  httpServer.start(-1);
  run_next_test();
}

function finish_test(request, buffer)
{
  httpServer.stop(do_test_finished);
}
