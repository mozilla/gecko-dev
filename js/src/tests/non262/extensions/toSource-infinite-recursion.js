// |reftest| skip-if(!Error.prototype.toSource)

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 650574;
var summary = 'Check for too-deep stack when converting a value to source';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var e = Error('');
e.fileName = e;
assertEq(e.toSource(), `(new Error("", {}, 18))`);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
