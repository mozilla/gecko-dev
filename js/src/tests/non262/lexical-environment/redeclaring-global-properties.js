// |reftest| skip-if(!xulRuntime.shell) -- needs evaluate
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Attempting to lexically redefine a var is a syntax error.
evaluate("var a;");
assertThrowsInstanceOf(() => evaluate("let a;"), SyntaxError);

// Attempting to lexically redefine a configurable global property that's not a
// var is okay.
this.b = 42;
assertEq(b, 42);
evaluate("let b = 17;");
assertEq(b, 17);


// Attempting to lexically redefine a var added by eval code is okay.
assertEq(typeof d, "undefined");
eval("var d = 33;");
assertEq(d, 33);

// Attempting to lexically redefine a var added by eval code, then deleted *as a
// name*, is okay.
assertEq(typeof e, "undefined");
eval("var e = 'ohia';");
assertEq(e, "ohia");
delete e;
assertEq(this.hasOwnProperty("e"), false);
evaluate("let e = 3.141592654;");
assertEq(e, 3.141592654);

// Attempting to lexically redefine a var added by eval code, then deleted *as a
// property*, is okay.
assertEq(typeof f, "undefined");
eval("var f = 8675309;");
assertEq(f, 8675309);
delete this.f;
assertEq(this.hasOwnProperty("f"), false);

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
