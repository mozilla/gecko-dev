function assertThrowsSyntaxError(x) {
    let success = false;
    try {
        eval(x);
        success = true;
    } catch (e) {
        assertEq(e instanceof SyntaxError, true);
    }
    assertEq(success, false);
}


assertThrowsSyntaxError("class X { x: 1 }")

if ('reportCompare' in this) {
    reportCompare(0, 0);
}
