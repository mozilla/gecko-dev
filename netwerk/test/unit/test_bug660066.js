/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
Components.utils.import("resource://gre/modules/NetUtil.jsm");
const SIMPLEURI_SPEC = "data:text/plain,hello world";
const BLOBURI_SPEC = "blob:123456";

function do_info(text, stack) {
  if (!stack)
    stack = Components.stack.caller;

  dump( "\n" +
       "TEST-INFO | " + stack.filename + " | [" + stack.name + " : " +
       stack.lineNumber + "] " + text + "\n");
}

function do_check_uri_neq(uri1, uri2)
{
  do_info("Checking equality in forward direction...");
  do_check_false(uri1.equals(uri2));
  do_check_false(uri1.equalsExceptRef(uri2));

  do_info("Checking equality in reverse direction...");
  do_check_false(uri2.equals(uri1));
  do_check_false(uri2.equalsExceptRef(uri1));
}

function run_test()
{
  var simpleURI = NetUtil.newURI(SIMPLEURI_SPEC);
  var fileDataURI = NetUtil.newURI(BLOBURI_SPEC);

  do_info("Checking that " + SIMPLEURI_SPEC + " != " + BLOBURI_SPEC);
  do_check_uri_neq(simpleURI, fileDataURI);

  do_info("Changing the nsSimpleURI spec to match the nsFileDataURI");
  simpleURI.spec = BLOBURI_SPEC;

  do_info("Verifying that .spec matches");
  do_check_eq(simpleURI.spec, fileDataURI.spec);

  do_info("Checking that nsSimpleURI != nsFileDataURI despite their .spec matching")
  do_check_uri_neq(simpleURI, fileDataURI);
}
