// SKIP test262 export
// https://github.com/tc39/ecma262/pull/2193
// Tests that IteratorReturn is called when a for-of loop has an abrupt
// completion value during non-iterator code.

function test() {
    var returnCalled = 0;
    var returnCalledExpected = 0;
    var iterable = {};
    iterable[Symbol.iterator] = makeIterator({
        ret: function() {
            returnCalled++;
            return {};
        }
    });

    // throw in lhs ref calls iter.return
    function throwlhs() {
        throw "in lhs";
    }
    assertThrowsValue(function() {
        for ((throwlhs()) of iterable)
            continue;
    }, "in lhs");
    assertEq(returnCalled, ++returnCalledExpected);
}

test();

if (typeof reportCompare === "function")
    reportCompare(0, 0);
