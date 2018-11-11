/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351102;
var summary = 'try/catch-guard/finally GC issues';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var f;
  try
  {
    try { d.d.d } catch([] if gc()) { }
  }
  catch(ex)
  {
  }
  reportCompare(expect, actual, summary + ': 5');

  exitFunc ('test');
}
