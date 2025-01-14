function testBasic() {
    var thisVal = {};
    var arr = [1, 2, 3];
    var f = function() {
        assertEq(this, thisVal);
        assertEq(arguments.length, 0);
        return 456;
    };
    var boundMathAbs = Math.abs.bind();
    var boundArrayJoin = Array.prototype.join.bind(arr);
    var boundArrayJoinArg = Array.prototype.join.bind(arr, "-");
    for (var i = 0; i < 100; i++) {
        // Scripted callee.
        assertEq(f.apply(thisVal), 456);
        assertEq(f.apply(thisVal), 456);
        assertEq(f.apply(thisVal, null), 456);
        assertEq(f.apply(thisVal, undefined), 456);

        // Native callee.
        assertEq(Math.abs.apply(), NaN);
        assertEq(Math.abs.apply(null), NaN);
        assertEq(Math.abs.apply(null, null), NaN);
        assertEq(Array.prototype.join.apply(arr), "1,2,3");
        assertEq(Array.prototype.join.apply(arr, null), "1,2,3");
        assertEq(Array.prototype.join.apply(arr, undefined), "1,2,3");

        // Bound native callee.
        assertEq(boundMathAbs.apply(), NaN);
        assertEq(boundMathAbs.apply(null), NaN);
        assertEq(boundMathAbs.apply(null, null), NaN);
        assertEq(boundMathAbs.apply(null, undefined), NaN);
        assertEq(boundArrayJoin.apply(), "1,2,3");
        assertEq(boundArrayJoin.apply(null), "1,2,3");
        assertEq(boundArrayJoin.apply(null, null), "1,2,3");
        assertEq(boundArrayJoin.apply(null, undefined), "1,2,3");
        assertEq(boundArrayJoinArg.apply(), "1-2-3");
        assertEq(boundArrayJoinArg.apply(null), "1-2-3");
        assertEq(boundArrayJoinArg.apply(null, null), "1-2-3");
        assertEq(boundArrayJoinArg.apply(null, undefined), "1-2-3");
    }
}
testBasic();

function testUndefinedGuard() {
    var f = function() { return arguments.length; }
    var arr = [-5, 5];
    var strings = ["a", "b"];
    var boundMathAbs = Math.abs.bind();
    var boundArrayJoin = Array.prototype.join.bind(strings);
    for (var i = 0; i < 100; i++) {
        var args = i < 50 ? undefined : arr;
        assertEq(f.apply(null, args), i < 50 ? 0 : 2);
        assertEq(Math.abs.apply(null, args), i < 50 ? NaN : 5);
        assertEq(Array.prototype.join.apply(strings, args), i < 50 ? "a,b" : "a-5b");
        assertEq(boundMathAbs.apply(null, args), i < 50 ? NaN : 5);
        assertEq(boundArrayJoin.apply(null, args), i < 50 ? "a,b" : "a-5b");
    }
}
testUndefinedGuard();
