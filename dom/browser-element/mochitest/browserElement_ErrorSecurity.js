/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 764718 - Test that mozbrowsererror works for a security error.

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var iframe = null;
 function runTest() {
  iframe = document.createElement('iframe');
  iframe.mozbrowser = true;
  document.body.appendChild(iframe);
 
  checkForGenericError();
}

function checkForGenericError() {
  iframe.addEventListener("mozbrowsererror", function onGenericError(e) {
    iframe.removeEventListener(e.type, onGenericError);
     ok(true, "Got mozbrowsererror event.");
    ok(e.detail.type == "other", "Event's detail has a |type| param with the value 'other'.");

    checkForExpiredCertificateError();
  });

  iframe.src = "http://this_is_not_a_domain.example.com";
}

function checkForExpiredCertificateError() {
  iframe.addEventListener("mozbrowsererror", function onCertError(e) {
    iframe.removeEventListener(e.type, onCertError);
    ok(true, "Got mozbrowsererror event.");
    ok(e.detail.type == "certerror", "Event's detail has a |type| param with the value 'certerror'.");

    checkForNoCertificateError();
  });

  iframe.src = "https://expired.example.com";
}


function checkForNoCertificateError() {
  iframe.addEventListener("mozbrowsererror", function onCertError(e) {
    iframe.removeEventListener(e.type, onCertError);
    ok(true, "Got mozbrowsererror event.");
    ok(e.detail.type == "certerror", "Event's detail has a |type| param with the value 'certerror'.");

    SimpleTest.finish();
  });
 
  iframe.src = "https://nocert.example.com";
}
 
addEventListener('testready', runTest);
