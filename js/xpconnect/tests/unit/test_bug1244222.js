/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

function run_test() {
  registerAppManifest(do_get_file('../components/js/xpctest.manifest'));

  // Generate a CCW to a function.
  var sb = new Cu.Sandbox(this);
  sb.eval('function fun(x) { return x; }');
  do_check_eq(sb.fun("foo"), "foo");

  // Double-wrap the CCW.
  var utils = Cc["@mozilla.org/js/xpc/test/js/TestUtils;1"].createInstance(Ci.nsIXPCTestUtils);
  var doubleWrapped = utils.doubleWrapFunction(sb.fun);
  do_check_eq(doubleWrapped.echo("foo"), "foo");

  // GC.
  Cu.forceGC();

  // Make sure it still works.
  do_check_eq(doubleWrapped.echo("foo"), "foo");
}
