Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;
// Need to randomize, because apparently no one clears our cache
var randomPath = "/redirect/" + Math.random();

XPCOMUtils.defineLazyGetter(this, "randomURI", function() {
  return URL + randomPath;
});

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
}

function redirectHandler(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 301, "Moved");
  response.setHeader("Location", "httpx://localhost:" +
                     httpServer.identity.primaryPort + "/content", false);
  response.setHeader("Cache-control", "max-age=1000", false);
}

function makeSureNotInCache(request, buffer)
{
  do_check_eq(request.status, Components.results.NS_ERROR_UNKNOWN_PROTOCOL);

  // It's very unlikely that we'd somehow succeed when we try again from cache.
  // Can't hurt to test though.
  var chan = make_channel(randomURI);
  chan.loadFlags |= Ci.nsIRequest.LOAD_ONLY_FROM_CACHE;
  chan.asyncOpen2(new ChannelListener(finish_test, null, CL_EXPECT_FAILURE));
}

function finish_test(request, buffer)
{
  do_check_eq(request.status, Components.results.NS_ERROR_UNKNOWN_PROTOCOL);
  do_check_eq(buffer, "");
  httpServer.stop(do_test_finished);
}

function run_test()
{
  httpServer = new HttpServer();
  httpServer.registerPathHandler(randomPath, redirectHandler);
  httpServer.start(-1);

  var chan = make_channel(randomURI);
  chan.asyncOpen2(new ChannelListener(makeSureNotInCache, null, CL_EXPECT_FAILURE));
  do_test_pending();
}
