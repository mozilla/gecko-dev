/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  let content = Cc[
    "@mozilla.org/network/android-content-input-stream;1"
  ].createInstance(Ci.nsIAndroidContentInputStream);
  let uri = Services.io.newURI(
    "content://org.mozilla.geckoview.test_runner.provider/blob"
  );
  content.init(uri);

  let sis = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
    Ci.nsIScriptableInputStream
  );
  sis.init(content);

  Assert.equal(sis.read(4), "ABCD", "data is valid");
}
