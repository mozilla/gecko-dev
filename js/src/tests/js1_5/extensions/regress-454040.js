/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 454040;
var summary = 'Do not crash @ js_ComputeFilename';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

try
{ 
  this.__defineGetter__("x", Function);
  this.__defineSetter__("x", Function);
  this.watch("x", x.__proto__);
  x = 1;
}
catch(ex)
{
}
reportCompare(expect, actual, summary);
