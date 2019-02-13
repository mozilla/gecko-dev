/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 741755 - Test that canGo{Back,Forward} and go{Forward,Back} work with
// <iframe mozbrowser>.

"use strict";
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var iframe;
function addOneShotIframeEventListener(event, fn) {
  function wrapper(e) {
    iframe.removeEventListener(event, wrapper);
    fn(e);
  };

  iframe.addEventListener(event, wrapper);
}

function runTest() {
  iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');

  addOneShotIframeEventListener('mozbrowserloadend', function() {
    SimpleTest.executeSoon(test2);
  });

  iframe.src = browserElementTestHelpers.emptyPage1;
  document.body.appendChild(iframe);
}

function checkCanGoBackAndForward(canGoBack, canGoForward, nextTest) {
  var seenCanGoBackResult = false;
  iframe.getCanGoBack().onsuccess = function(e) {
    is(seenCanGoBackResult, false, "onsuccess handler shouldn't be called twice.");
    seenCanGoBackResult = true;
    is(e.target.result, canGoBack);
    maybeRunNextTest();
  };

  var seenCanGoForwardResult = false;
  iframe.getCanGoForward().onsuccess = function(e) {
    is(seenCanGoForwardResult, false, "onsuccess handler shouldn't be called twice.");
    seenCanGoForwardResult = true;
    is(e.target.result, canGoForward);
    maybeRunNextTest();
  };

  function maybeRunNextTest() {
    if (seenCanGoBackResult && seenCanGoForwardResult) {
      nextTest();
    }
  }
}

function test2() {
  checkCanGoBackAndForward(false, false, test3);
}

function test3() {
  addOneShotIframeEventListener('mozbrowserloadend', function() {
    checkCanGoBackAndForward(true, false, test4);
  });

  SimpleTest.executeSoon(function() {
    iframe.src = browserElementTestHelpers.emptyPage2;
  });
}

function test4() {
  addOneShotIframeEventListener('mozbrowserlocationchange', function(e) {
    is(e.detail, browserElementTestHelpers.emptyPage3);
    checkCanGoBackAndForward(true, false, test5);
  });

  SimpleTest.executeSoon(function() {
    iframe.src = browserElementTestHelpers.emptyPage3;
  });
}

function test5() {
  addOneShotIframeEventListener('mozbrowserlocationchange', function(e) {
    is(e.detail, browserElementTestHelpers.emptyPage2);
    checkCanGoBackAndForward(true, true, test6);
  });
  iframe.goBack();
}

function test6() {
  addOneShotIframeEventListener('mozbrowserlocationchange', function(e) {
    is(e.detail, browserElementTestHelpers.emptyPage1);
    checkCanGoBackAndForward(false, true, SimpleTest.finish);
  });
  iframe.goBack();
}

addEventListener('testready', runTest);
