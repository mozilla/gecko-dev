"use strict";

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function run_test() {
  run_next_test();
}

// Test that OS.Constants is defined correctly.
add_task(function* check_definition() {
  do_check_true(OS.Constants!=null);
  do_check_true(!!OS.Constants.Win || !!OS.Constants.libc);
  do_check_true(OS.Constants.Path!=null);
  do_check_true(OS.Constants.Sys!=null);
  do_check_eq(Services.appinfo.OS, "XPCShell");

  //check if using DEBUG build
  if (Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2).isDebugBuild == true) {
    do_check_true(OS.Constants.Sys.DEBUG);
  } else {
    do_check_true(typeof(OS.Constants.Sys.DEBUG) == 'undefined');
  }
});
