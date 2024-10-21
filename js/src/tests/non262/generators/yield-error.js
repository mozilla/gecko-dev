var BUGNUMBER = 1384299;
var summary = "yield outside of generators should provide better error";

print(BUGNUMBER + ": " + summary);

assertThrowsInstanceOfWithMessage(
    () => eval("yield 10"),
    SyntaxError,
    "yield expression is only valid in generators");

assertThrowsInstanceOfWithMessage(
    () => eval("yield 10"),
    SyntaxError,
    "yield expression is only valid in generators");

assertThrowsInstanceOfWithMessage(
    () => eval("yield 10"),
    SyntaxError,
    "yield expression is only valid in generators");

if (typeof reportCompare === "function")
    reportCompare(true, true);
