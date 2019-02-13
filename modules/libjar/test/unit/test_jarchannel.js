/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests some basic jar channel functionality
 */


const {classes: Cc,
       interfaces: Ci,
       results: Cr,
       utils: Cu,
       Constructor: ctor
       } = Components;

Cu.import("resource://gre/modules/Services.jsm");

const ios = Cc["@mozilla.org/network/io-service;1"].
                getService(Ci.nsIIOService);
const dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                getService(Ci.nsIProperties);
const obs = Cc["@mozilla.org/observer-service;1"].
                getService(Ci.nsIObserverService);

const nsIBinaryInputStream = ctor("@mozilla.org/binaryinputstream;1",
                               "nsIBinaryInputStream",
                               "setInputStream"
                               );

const fileBase = "test_bug637286.zip";
const file = do_get_file("data/" + fileBase);
// on child we'll test with jar:remoteopenfile:// instead of jar:file://
const jarBase = "jar:" + filePrefix + ios.newFileURI(file).spec + "!";
const tmpDir = dirSvc.get("TmpD", Ci.nsIFile);

function Listener(callback) {
    this._callback = callback;
}
Listener.prototype = {
    gotStartRequest: false,
    available: -1,
    gotStopRequest: false,
    QueryInterface: function(iid) {
        if (iid.equals(Ci.nsISupports) ||
            iid.equals(Ci.nsIRequestObserver))
            return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
    },
    onDataAvailable: function(request, ctx, stream, offset, count) {
        try {
            this.available = stream.available();
            do_check_eq(this.available, count);
            // Need to consume stream to avoid assertion
            new nsIBinaryInputStream(stream).readBytes(count);
        }
        catch (ex) {
            do_throw(ex);
        }
    },
    onStartRequest: function(request, ctx) {
        this.gotStartRequest = true;
    },
    onStopRequest: function(request, ctx, status) {
        this.gotStopRequest = true;
        do_check_eq(status, 0);
        if (this._callback) {
            this._callback.call(null, this);
        }
    }
};

/**
 * Basic reading test for asynchronously opened jar channel
 */
function testAsync() {
    var uri = jarBase + "/inner40.zip";
    var chan = ios.newChannel2(uri,
                               null,
                               null,
                               null,      // aLoadingNode
                               Services.scriptSecurityManager.getSystemPrincipal(),
                               null,      // aTriggeringPrincipal
                               Ci.nsILoadInfo.SEC_NORMAL,
                               Ci.nsIContentPolicy.TYPE_OTHER);
    do_check_true(chan.contentLength < 0);
    chan.asyncOpen(new Listener(function(l) {
        do_check_true(chan.contentLength > 0);
        do_check_true(l.gotStartRequest);
        do_check_true(l.gotStopRequest);
        do_check_eq(l.available, chan.contentLength);

        run_next_test();
    }), null);
}

add_test(testAsync);
// Run same test again so we test the codepath for a zipcache hit
add_test(testAsync);

/**
 * Basic test for nsIZipReader.
 * This relies on the jar cache to succeed in child processes.
 */
function testZipEntry() {
    var uri = jarBase + "/inner40.zip";
    var chan = ios.newChannel2(uri,
                               null,
                               null,
                               null,      // aLoadingNode
                               Services.scriptSecurityManager.getSystemPrincipal(),
                               null,      // aTriggeringPrincipal
                               Ci.nsILoadInfo.SEC_NORMAL,
                               Ci.nsIContentPolicy.TYPE_OTHER)
                  .QueryInterface(Ci.nsIJARChannel);
    var entry = chan.zipEntry;
    do_check_true(entry.CRC32 == 0x8b635486);
    do_check_true(entry.realSize == 184);
    run_next_test();
}

add_test(testZipEntry);

// In e10s child processes we don't currently support 
// 1) synchronously opening jar files on parent
// 2) nested jar channels in e10s: (app:// doesn't use them).
// 3) we can't do file lock checks on android, so skip those tests too.
if (!inChild) {

  /**
   * Basic reading test for synchronously opened jar channels
   */
  add_test(function testSync() {
      var uri = jarBase + "/inner40.zip";
      var chan = ios.newChannel2(uri,
                                 null,
                                 null,
                                 null,      // aLoadingNode
                                 Services.scriptSecurityManager.getSystemPrincipal(),
                                 null,      // aTriggeringPrincipal
                                 Ci.nsILoadInfo.SEC_NORMAL,
                                 Ci.nsIContentPolicy.TYPE_OTHER);
      var stream = chan.open();
      do_check_true(chan.contentLength > 0);
      do_check_eq(stream.available(), chan.contentLength);
      stream.close();
      stream.close(); // should still not throw

      run_next_test();
  });


  /**
   * Basic reading test for synchronously opened, nested jar channels
   */
  add_test(function testSyncNested() {
      var uri = "jar:" + jarBase + "/inner40.zip!/foo";
      var chan = ios.newChannel2(uri,
                                 null,
                                 null,
                                 null,      // aLoadingNode
                                 Services.scriptSecurityManager.getSystemPrincipal(),
                                 null,      // aTriggeringPrincipal
                                 Ci.nsILoadInfo.SEC_NORMAL,
                                 Ci.nsIContentPolicy.TYPE_OTHER);
      var stream = chan.open();
      do_check_true(chan.contentLength > 0);
      do_check_eq(stream.available(), chan.contentLength);
      stream.close();
      stream.close(); // should still not throw

      run_next_test();
  });

  /**
   * Basic reading test for asynchronously opened, nested jar channels
   */
  add_test(function testAsyncNested(next) {
      var uri = "jar:" + jarBase + "/inner40.zip!/foo";
      var chan = ios.newChannel2(uri,
                                 null,
                                 null,
                                 null,      // aLoadingNode
                                 Services.scriptSecurityManager.getSystemPrincipal(),
                                 null,      // aTriggeringPrincipal
                                 Ci.nsILoadInfo.SEC_NORMAL,
                                 Ci.nsIContentPolicy.TYPE_OTHER);
      chan.asyncOpen(new Listener(function(l) {
          do_check_true(chan.contentLength > 0);
          do_check_true(l.gotStartRequest);
          do_check_true(l.gotStopRequest);
          do_check_eq(l.available, chan.contentLength);

          run_next_test();
      }), null);
  });

  /**
   * Verify that file locks are released when closing a synchronously
   * opened jar channel stream
   */
  add_test(function testSyncCloseUnlocks() {
      var copy = tmpDir.clone();
      copy.append(fileBase);
      file.copyTo(copy.parent, copy.leafName);

      var uri = "jar:" + ios.newFileURI(copy).spec + "!/inner40.zip";
      var chan = ios.newChannel2(uri,
                                 null,
                                 null,
                                 null,      // aLoadingNode
                                 Services.scriptSecurityManager.getSystemPrincipal(),
                                 null,      // aTriggeringPrincipal
                                 Ci.nsILoadInfo.SEC_NORMAL,
                                 Ci.nsIContentPolicy.TYPE_OTHER);
      var stream = chan.open();
      do_check_true(chan.contentLength > 0);
      stream.close();

      // Drop any jar caches
      obs.notifyObservers(null, "chrome-flush-caches", null);

      try {
          copy.remove(false);
      }
      catch (ex) {
          do_throw(ex);
      }

      run_next_test();
  });

  /**
   * Verify that file locks are released when closing an asynchronously
   * opened jar channel stream
   */
  add_test(function testAsyncCloseUnlocks() {
      var copy = tmpDir.clone();
      copy.append(fileBase);
      file.copyTo(copy.parent, copy.leafName);

      var uri = "jar:" + ios.newFileURI(copy).spec + "!/inner40.zip";
      var chan = ios.newChannel2(uri,
                                 null,
                                 null,
                                 null,      // aLoadingNode
                                 Services.scriptSecurityManager.getSystemPrincipal(),
                                 null,      // aTriggeringPrincipal
                                 Ci.nsILoadInfo.SEC_NORMAL,
                                 Ci.nsIContentPolicy.TYPE_OTHER);
      chan.asyncOpen(new Listener(function (l) {
          do_check_true(chan.contentLength > 0);

          // Drop any jar caches
          obs.notifyObservers(null, "chrome-flush-caches", null);

          try {
              copy.remove(false);
          }
          catch (ex) {
              do_throw(ex);
          }

          run_next_test();
      }), null);
  });

} // if !inChild

if (inChild) {
    /**
     * Multiple simultaneous opening test for bug 1048615
     */
    add_test(function testSimultaneous() {
        var uri = jarBase + "/inner1.zip";

        // Drop any JAR caches
        obs.notifyObservers(null, "chrome-flush-caches", null);

        // Open the first channel without ensureChildFd()
        var chan_first = ios.newChannel2(uri,
                                         null,
                                         null,
                                         null,      // aLoadingNode
                                         Services.scriptSecurityManager.getSystemPrincipal(),
                                         null,      // aTriggeringPrincipal
                                         Ci.nsILoadInfo.SEC_NORMAL,
                                         Ci.nsIContentPolicy.TYPE_OTHER)
                            .QueryInterface(Ci.nsIJARChannel);
        chan_first.asyncOpen(new Listener(function(l) {
        }), null);

        // Open multiple channels with ensureChildFd()
        var num = 10;
        var chan = [];
        for (var i = 0; i < num; i++) {
            chan[i] = ios.newChannel2(uri,
                                      null,
                                      null,
                                      null,      // aLoadingNode
                                      Services.scriptSecurityManager.getSystemPrincipal(),
                                      null,      // aTriggeringPrincipal
                                      Ci.nsILoadInfo.SEC_NORMAL,
                                      Ci.nsIContentPolicy.TYPE_OTHER)
                         .QueryInterface(Ci.nsIJARChannel);
            chan[i].ensureChildFd();
            chan[i].asyncOpen(new Listener(function(l) {
            }), null);
        }

        // Open the last channel with ensureChildFd()
        var chan_last = ios.newChannel2(uri,
                                        null,
                                        null,
                                        null,      // aLoadingNode
                                        Services.scriptSecurityManager.getSystemPrincipal(),
                                        null,      // aTriggeringPrincipal
                                        Ci.nsILoadInfo.SEC_NORMAL,
                                        Ci.nsIContentPolicy.TYPE_OTHER)
                           .QueryInterface(Ci.nsIJARChannel);
        chan_last.ensureChildFd();
        chan_last.asyncOpen(new Listener(function(l) {
            run_next_test();
        }), null);
    });
} // if inChild

function run_test() run_next_test();
