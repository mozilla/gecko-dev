/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that window.close() works.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  document.body.appendChild(iframe);

  iframe.addEventListener("mozbrowserclose", function(e) {
    ok(true, "got mozbrowserclose event.");
    SimpleTest.finish();
  });

  iframe.src = "data:text/html,<html><body><script>window.close()</scr"+"ipt></body></html>";
}

addEventListener('testready', runTest);
