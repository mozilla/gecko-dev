/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 328897;
var summary = 'JS_ReportPendingException should';

var actual = 'No Error';
var expect = 'No Error';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
if (typeof window == 'undefined')
{
  reportCompare(expect, actual, summary);
}
else
{
  expect = /TypeError: Not enough arguments to Window.showModalDialog./;

  window._onerror = window.onerror;
  window.onerror = (function (msg, page, line) { 
      actual = msg; 
      gDelayTestDriverEnd = false;
      jsTestDriverEnd();
      reportMatch(expect, actual, summary);
    });

  gDelayTestDriverEnd = true;

  // Trying to set call window.showModalDialog() without any arguments will throw.
  window.showModalDialog();
  actual = 'No Error';
}

function onload() 
{
  if (actual == 'No Error')
  {
    gDelayTestDriverEnd = false;
    jsTestDriverEnd();
    reportCompare(expect, actual, summary);
  }
}
