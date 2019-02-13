Cu.import("resource://gre/modules/Services.jsm");

var Cc = Components.classes;
var Ci = Components.interfaces;

var listener = {
  expect_failure: false,
  QueryInterface: function listener_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIUnicharStreamLoaderObserver)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  onDetermineCharset : function onDetermineCharset(loader, context, data)
  {
    return "us-ascii";
  },
  onStreamComplete : function onStreamComplete (loader, context, status, data)
  {
    try {
      if (this.expect_failure)
        do_check_false(Components.isSuccessCode(status));
      else
        do_check_eq(status, Components.results.NS_OK);
      do_check_eq(data, "");
      do_check_neq(loader.channel, null);
      tests[current_test++]();
    } finally {
      do_test_finished();
    }
  }
};

var current_test = 0;
var tests = [test1, test2, done];

function run_test() {
  tests[current_test++]();
}

function test1() {
  var f =
      Cc["@mozilla.org/network/unichar-stream-loader;1"].
      createInstance(Ci.nsIUnicharStreamLoader);
  f.init(listener);

  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  var chan = ios.newChannel2("data:text/plain,",
                             null,
                             null,
                             null,      // aLoadingNode
                             Services.scriptSecurityManager.getSystemPrincipal(),
                             null,      // aTriggeringPrincipal
                             Ci.nsILoadInfo.SEC_NORMAL,
                             Ci.nsIContentPolicy.TYPE_OTHER);
  chan.asyncOpen(f, null);
  do_test_pending();
}

function test2() {
  var f =
      Cc["@mozilla.org/network/unichar-stream-loader;1"].
      createInstance(Ci.nsIUnicharStreamLoader);
  f.init(listener);

  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  var chan = ios.newChannel2("http://localhost:0/",
                             null,
                             null,
                             null,      // aLoadingNode
                             Services.scriptSecurityManager.getSystemPrincipal(),
                             null,      // aTriggeringPrincipal
                             Ci.nsILoadInfo.SEC_NORMAL,
                             Ci.nsIContentPolicy.TYPE_OTHER);
  listener.expect_failure = true;
  chan.asyncOpen(f, null);
  do_test_pending();
}

function done() {
}
