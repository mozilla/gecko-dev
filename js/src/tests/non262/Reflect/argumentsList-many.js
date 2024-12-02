/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// SKIP test262 export
// Testing unspecified implementation limits.

// Tests for the argumentList argument to Reflect.apply and Reflect.construct.
// Many arguments can be passed.
var many = 65537;
var args = {length: many, 0: "zero", [many - 1]: "last"};
function testMany(...args) {
    for (var i = 0; i < many; i++) {
        assertEq(i in args, true);
        assertEq(args[i], i === 0 ? "zero" : i === many - 1 ? "last" : undefined);
    }
    return this;
}
assertEq(Reflect.apply(testMany, "pass", args).toString(), "pass");
assertEq(Reflect.construct(testMany, args) instanceof testMany, true);
assertEq(Reflect.apply(new Proxy(testMany, {}), "pass", args).toString(), "pass");
assertEq(Reflect.construct(new Proxy(testMany, {}), args) instanceof testMany, true);

// If argumentsList.length is unreasonably huge, we get an error.
// (This is an implementation limit.)
var BOTH = [
    Reflect.apply,
    // Adapt Reflect.construct to accept the same arguments as Reflect.apply.
    (target, thisArgument, argumentList) => Reflect.construct(target, argumentList)
];

for (var method of BOTH) {
    for (var value of [1e12, 1e240, Infinity]) {
        assertThrowsInstanceOf(() => method(count, undefined, {length: value}),
                               Error);
    }
}

reportCompare(0, 0);
