/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
Cu.import("resource://gre/modules/NetUtil.jsm");

// Check that the zip cache can expire entries from nested jars
var ios = Cc["@mozilla.org/network/io-service;1"].
          getService(Ci.nsIIOService);

function open_inner_zip(base, idx) {
    var spec = "jar:" + base + "inner" + idx + ".zip!/foo";
    var channel = NetUtil.newChannel({uri: spec, loadUsingSystemPrincipal: true});
    var stream = channel.open2();
}

function run_test() {
  var file = do_get_file("data/test_bug637286.zip");
  var outerJarBase = "jar:" + ios.newFileURI(file).spec + "!/";

  for (var i = 0; i < 40; i++) {
    open_inner_zip(outerJarBase, i);
    gc();
  }
}

