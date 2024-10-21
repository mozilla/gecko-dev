var BUGNUMBER = 1391519;
var summary = "for-await-of outside of async function should provide better error";

print(BUGNUMBER + ": " + summary);

assertThrowsInstanceOfWithMessageContains(
    () => eval("for await (let x of []) {}"),
    SyntaxError,
    "for await (... of ...) is only valid in"
);

// Extra `await` shouldn't throw that error.
assertThrowsInstanceOfWithMessageContains(
    () => eval("async function f() { for await await (let x of []) {} }"),
    SyntaxError,
    "missing ( after for"
);

if (typeof reportCompare === "function")
    reportCompare(true, true);
