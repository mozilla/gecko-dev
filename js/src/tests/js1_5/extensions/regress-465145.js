/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465145;
var summary = 'Do not crash with watched defined setter';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 

this.__defineSetter__("x", function(){});
this.watch("x", function(){});
y = this;
for (var z = 0; z < 2; ++z) { x = y };
this.__defineSetter__("x", function(){});
for (var z = 0; z < 2; ++z) { x = y };


reportCompare(expect, actual, summary);
