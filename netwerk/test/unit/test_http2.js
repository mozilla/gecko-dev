// test HTTP/2

var Ci = Components.interfaces;
var Cc = Components.classes;

// Generate a small and a large post with known pre-calculated md5 sums
function generateContent(size) {
  var content = "";
  for (var i = 0; i < size; i++) {
    content += "0";
  }
  return content;
}

var posts = [];
posts.push(generateContent(10));
posts.push(generateContent(250000));

// pre-calculated md5sums (in hex) of the above posts
var md5s = ['f1b708bba17f1ce948dc979f4d7092bc',
            '2ef8d3b6c8f329318eb1a119b12622b6'];

var bigListenerData = generateContent(128 * 1024);
var bigListenerMD5 = '8f607cfdd2c87d6a7eedb657dafbd836';

function checkIsHttp2(request) {
  try {
    if (request.getResponseHeader("X-Firefox-Spdy") == "h2-12") {
      if (request.getResponseHeader("X-Connection-Http2") == "yes") {
        return true;
      }
      return false; // Weird case, but the server disagrees with us
    }
  } catch (e) {
    // Nothing to do here
  }
  return false;
}

var Http2CheckListener = function() {};

Http2CheckListener.prototype = {
  onStartRequestFired: false,
  onDataAvailableFired: false,
  isHttp2Connection: false,

  onStartRequest: function testOnStartRequest(request, ctx) {
    this.onStartRequestFired = true;

    if (!Components.isSuccessCode(request.status))
      do_throw("Channel should have a success code! (" + request.status + ")");
    if (!(request instanceof Components.interfaces.nsIHttpChannel))
      do_throw("Expecting an HTTP channel");

    do_check_eq(request.responseStatus, 200);
    do_check_eq(request.requestSucceeded, true);
  },

  onDataAvailable: function testOnDataAvailable(request, ctx, stream, off, cnt) {
    this.onDataAvailableFired = true;
    this.isHttp2Connection = checkIsHttp2(request);

    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, ctx, status) {
    do_check_true(this.onStartRequestFired);
    do_check_true(this.onDataAvailableFired);
    do_check_true(this.isHttp2Connection);

    run_next_test();
    do_test_finished();
  }
};

/*
 * Support for testing valid multiplexing of streams
 */

var multiplexContent = generateContent(30*1024);
var completed_channels = [];
function register_completed_channel(listener) {
  completed_channels.push(listener);
  if (completed_channels.length == 2) {
    do_check_neq(completed_channels[0].streamID, completed_channels[1].streamID);
    run_next_test();
    do_test_finished();
  }
}

/* Listener class to control the testing of multiplexing */
var Http2MultiplexListener = function() {};

Http2MultiplexListener.prototype = new Http2CheckListener();

Http2MultiplexListener.prototype.streamID = 0;
Http2MultiplexListener.prototype.buffer = "";

Http2MultiplexListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  this.streamID = parseInt(request.getResponseHeader("X-Http2-StreamID"));
  var data = read_stream(stream, cnt);
  this.buffer = this.buffer.concat(data);
};

Http2MultiplexListener.prototype.onStopRequest = function(request, ctx, status) {
  do_check_true(this.onStartRequestFired);
  do_check_true(this.onDataAvailableFired);
  do_check_true(this.isHttp2Connection);
  do_check_true(this.buffer == multiplexContent);
  
  // This is what does most of the hard work for us
  register_completed_channel(this);
};

// Does the appropriate checks for header gatewaying
var Http2HeaderListener = function(name, callback) {
  this.name = name;
  this.callback = callback;
};

Http2HeaderListener.prototype = new Http2CheckListener();
Http2HeaderListener.prototype.value = "";

Http2HeaderListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  var hvalue = request.getResponseHeader(this.name);
  do_check_neq(hvalue, "");
  this.callback(hvalue);
  read_stream(stream, cnt);
};

var Http2PushListener = function() {};

Http2PushListener.prototype = new Http2CheckListener();

Http2PushListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  if (ctx.originalURI.spec == "https://localhost:6944/push.js" ||
      ctx.originalURI.spec == "https://localhost:6944/push2.js") {
    do_check_eq(request.getResponseHeader("pushed"), "yes");
  }
  read_stream(stream, cnt);
};

// Does the appropriate checks for a large GET response
var Http2BigListener = function() {};

Http2BigListener.prototype = new Http2CheckListener();
Http2BigListener.prototype.buffer = "";

Http2BigListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  this.buffer = this.buffer.concat(read_stream(stream, cnt));
  // We know the server should send us the same data as our big post will be,
  // so the md5 should be the same
  do_check_eq(bigListenerMD5, request.getResponseHeader("X-Expected-MD5"));
};

Http2BigListener.prototype.onStopRequest = function(request, ctx, status) {
  do_check_true(this.onStartRequestFired);
  do_check_true(this.onDataAvailableFired);
  do_check_true(this.isHttp2Connection);

  // Don't want to flood output, so don't use do_check_eq
  do_check_true(this.buffer == bigListenerData);

  run_next_test();
  do_test_finished();
};

// Does the appropriate checks for POSTs
var Http2PostListener = function(expected_md5) {
  this.expected_md5 = expected_md5;
};

Http2PostListener.prototype = new Http2CheckListener();
Http2PostListener.prototype.expected_md5 = "";

Http2PostListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  read_stream(stream, cnt);
  do_check_eq(this.expected_md5, request.getResponseHeader("X-Calculated-MD5"));
};

function makeChan(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel(url, null, null).QueryInterface(Ci.nsIHttpChannel);

  return chan;
}

// Make sure we make a HTTP2 connection and both us and the server mark it as such
function test_http2_basic() {
  var chan = makeChan("https://localhost:6944/");
  var listener = new Http2CheckListener();
  chan.asyncOpen(listener, null);
}

// Support for making sure XHR works over SPDY
function checkXhr(xhr) {
  if (xhr.readyState != 4) {
    return;
  }

  do_check_eq(xhr.status, 200);
  do_check_eq(checkIsHttp2(xhr), true);
  run_next_test();
  do_test_finished();
}

// Fires off an XHR request over SPDY
function test_http2_xhr() {
  var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  req.open("GET", "https://localhost:6944/", true);
  req.addEventListener("readystatechange", function (evt) { checkXhr(req); },
                       false);
  req.send(null);
}

// Test to make sure we get multiplexing right
function test_http2_multiplex() {
  var chan1 = makeChan("https://localhost:6944/multiplex1");
  var chan2 = makeChan("https://localhost:6944/multiplex2");
  var listener1 = new Http2MultiplexListener();
  var listener2 = new Http2MultiplexListener();
  chan1.asyncOpen(listener1, null);
  chan2.asyncOpen(listener2, null);
}

// Test to make sure we gateway non-standard headers properly
function test_http2_header() {
  var chan = makeChan("https://localhost:6944/header");
  var hvalue = "Headers are fun";
  chan.setRequestHeader("X-Test-Header", hvalue, false);
  var listener = new Http2HeaderListener("X-Received-Test-Header", function(received_hvalue) {
    do_check_eq(received_hvalue, hvalue);
  });
  chan.asyncOpen(listener, null);
}

// Test to make sure cookies are split into separate fields before compression
function test_http2_cookie_crumbling() {
  var chan = makeChan("https://localhost:6944/cookie_crumbling");
  var cookiesSent = ['a=b', 'c=d', 'e=f'].sort();
  chan.setRequestHeader("Cookie", cookiesSent.join('; '), false);
  var listener = new Http2HeaderListener("X-Received-Header-Pairs", function(pairsReceived) {
    var cookiesReceived = JSON.parse(pairsReceived).filter(function(pair) {
      return pair[0] == 'cookie';
    }).map(function(pair) {
      return pair[1];
    }).sort();
    do_check_eq(cookiesReceived.length, cookiesSent.length);
    cookiesReceived.forEach(function(cookieReceived, index) {
      do_check_eq(cookiesSent[index], cookieReceived)
    });
  });
  chan.asyncOpen(listener, null);
}

function test_http2_push1() {
  var chan = makeChan("https://localhost:6944/push");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener();
  chan.asyncOpen(listener, chan);
}

function test_http2_push2() {
  var chan = makeChan("https://localhost:6944/push.js");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener();
  chan.asyncOpen(listener, chan);
}

function test_http2_push3() {
  var chan = makeChan("https://localhost:6944/push2");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener();
  chan.asyncOpen(listener, chan);
}

function test_http2_push4() {
  var chan = makeChan("https://localhost:6944/push2.js");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener();
  chan.asyncOpen(listener, chan);
}

// this is a basic test where the server sends a simple document with 2 header
// blocks. bug 1027364
function test_http2_doubleheader() {
  var chan = makeChan("https://localhost:6944/doubleheader");
  var listener = new Http2CheckListener();
  chan.asyncOpen(listener, null);
}

// Make sure we handle GETs that cover more than 2 frames properly
function test_http2_big() {
  var chan = makeChan("https://localhost:6944/big");
  var listener = new Http2BigListener();
  chan.asyncOpen(listener, null);
}

// Support for doing a POST
function do_post(content, chan, listener) {
  var stream = Cc["@mozilla.org/io/string-input-stream;1"]
               .createInstance(Ci.nsIStringInputStream);
  stream.data = content;

  var uchan = chan.QueryInterface(Ci.nsIUploadChannel);
  uchan.setUploadStream(stream, "text/plain", stream.available());

  chan.requestMethod = "POST";

  chan.asyncOpen(listener, null);
}

// Make sure we can do a simple POST
function test_http2_post() {
  var chan = makeChan("https://localhost:6944/post");
  var listener = new Http2PostListener(md5s[0]);
  do_post(posts[0], chan, listener);
}

// Make sure we can do a POST that covers more than 2 frames
function test_http2_post_big() {
  var chan = makeChan("https://localhost:6944/post");
  var listener = new Http2PostListener(md5s[1]);
  do_post(posts[1], chan, listener);
}

// hack - the header test resets the multiplex object on the server,
// so make sure header is always run before the multiplex test.
//
// make sure post_big runs first to test race condition in restarting
// a stalled stream when a SETTINGS frame arrives
var tests = [ test_http2_post_big
            , test_http2_basic
            , test_http2_push1
            , test_http2_push2
            , test_http2_push3
            , test_http2_push4
            , test_http2_doubleheader
            , test_http2_xhr
            , test_http2_header
            , test_http2_cookie_crumbling
            , test_http2_multiplex
            , test_http2_big
            , test_http2_post
            ];
var current_test = 0;

function run_next_test() {
  if (current_test < tests.length) {
    tests[current_test]();
    current_test++;
    do_test_pending();
  }
}

// Support for making sure we can talk to the invalid cert the server presents
var CertOverrideListener = function(host, port, bits) {
  this.host = host;
  if (port) {
    this.port = port;
  }
  this.bits = bits;
};

CertOverrideListener.prototype = {
  host: null,
  port: -1,
  bits: null,

  getInterface: function(aIID) {
    return this.QueryInterface(aIID);
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIBadCertListener2) ||
        aIID.equals(Ci.nsIInterfaceRequestor) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  notifyCertProblem: function(socketInfo, sslStatus, targetHost) {
    var cert = sslStatus.QueryInterface(Ci.nsISSLStatus).serverCert;
    var cos = Cc["@mozilla.org/security/certoverride;1"].
              getService(Ci.nsICertOverrideService);
    cos.rememberValidityOverride(this.host, this.port, cert, this.bits, false);
    return true;
  },
};

function addCertOverride(host, port, bits) {
  var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  try {
    var url;
    if (port) {
      url = "https://" + host + ":" + port + "/";
    } else {
      url = "https://" + host + "/";
    }
    req.open("GET", url, false);
    req.channel.notificationCallbacks = new CertOverrideListener(host, port, bits);
    req.send(null);
  } catch (e) {
    // This will fail since the server is not trusted yet
  }
}

var prefs;
var spdypref;
var spdy3pref;
var spdypush;
var http2pref;
var tlspref;

var loadGroup;

function resetPrefs() {
  prefs.setBoolPref("network.http.spdy.enabled", spdypref);
  prefs.setBoolPref("network.http.spdy.enabled.v3", spdy3pref);
  prefs.setBoolPref("network.http.spdy.allow-push", spdypush);
  prefs.setBoolPref("network.http.spdy.enabled.http2draft", http2pref);
  prefs.setBoolPref("network.http.spdy.enforce-tls-profile", tlspref);
}

function run_test() {
  // Set to allow the cert presented by our SPDY server
  do_get_profile();
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  var oldPref = prefs.getIntPref("network.http.speculative-parallel-limit");
  prefs.setIntPref("network.http.speculative-parallel-limit", 0);

  addCertOverride("localhost", 6944,
                  Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                  Ci.nsICertOverrideService.ERROR_MISMATCH |
                  Ci.nsICertOverrideService.ERROR_TIME);

  prefs.setIntPref("network.http.speculative-parallel-limit", oldPref);

  // Enable all versions of spdy to see that we auto negotiate http/2
  spdypref = prefs.getBoolPref("network.http.spdy.enabled");
  spdy3pref = prefs.getBoolPref("network.http.spdy.enabled.v3");
  spdypush = prefs.getBoolPref("network.http.spdy.allow-push");
  http2pref = prefs.getBoolPref("network.http.spdy.enabled.http2draft");
  tlspref = prefs.getBoolPref("network.http.spdy.enforce-tls-profile");
  prefs.setBoolPref("network.http.spdy.enabled", true);
  prefs.setBoolPref("network.http.spdy.enabled.v3", true);
  prefs.setBoolPref("network.http.spdy.allow-push", true);
  prefs.setBoolPref("network.http.spdy.enabled.http2draft", true);
  prefs.setBoolPref("network.http.spdy.enforce-tls-profile", false);

  loadGroup = Cc["@mozilla.org/network/load-group;1"].createInstance(Ci.nsILoadGroup);

  // And make go!
  run_next_test();
}
