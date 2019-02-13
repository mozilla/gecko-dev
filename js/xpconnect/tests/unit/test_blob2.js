/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.importGlobalProperties(['Blob', 'File']);

const Ci = Components.interfaces;

function run_test() {
  // throw if anything goes wrong
  let testContent = "<a id=\"a\"><b id=\"b\">hey!<\/b><\/a>";
  // should be able to construct a file
  var f1 = new Blob([testContent], {"type" : "text/xml"});

  // do some tests
  do_check_true(f1 instanceof Ci.nsIDOMBlob, "Should be a DOM Blob");

  do_check_true(!(f1 instanceof File), "Should not be a DOM File");

  do_check_true(f1.type == "text/xml", "Wrong type");

  do_check_true(f1.size == testContent.length, "Wrong content size");

  var f2 = new Blob();
  do_check_true(f2.size == 0, "Wrong size");
  do_check_true(f2.type == "", "Wrong type");

  var threw = false;
  try {
    // Needs a valid ctor argument
    var f2 = new Blob(Date(132131532));
  } catch (e) {
    threw = true;
  }
  do_check_true(threw, "Passing a random object should fail");
}
