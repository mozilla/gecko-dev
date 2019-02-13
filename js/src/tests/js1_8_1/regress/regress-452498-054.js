/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452498;
var summary = 'TM: upvar2 regression tests';
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


// ------- Comment #54 From Jason Orendorff

// Assertion failure: cg->flags & TCF_IN_FUNCTION
// at ../jsemit.cpp:1991
//
  function f() { var x; eval("let x, x;"); }
  f();

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
