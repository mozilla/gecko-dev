// |reftest| skip-if(!xulRuntime.shell)

assertThrowsInstanceOf(() => eval(`class A { #x; #x; }`), SyntaxError);

// No computed private fields
assertThrowsInstanceOf(
    () => eval(`var x = "foo"; class A { #[x] = 20; }`), SyntaxError);

assertThrowsInstanceOfWithMessage(() => eval(`class A { #x; h(o) { return !#x; }}`),
    SyntaxError,
    "invalid use of private name in unary expression without object reference");
assertThrowsInstanceOfWithMessage(() => eval(`class A { #x; h(o) { return 1 + #x in o; }}`),
    SyntaxError,
    "invalid use of private name due to operator precedence");


if (typeof reportCompare === 'function') reportCompare(0, 0);
