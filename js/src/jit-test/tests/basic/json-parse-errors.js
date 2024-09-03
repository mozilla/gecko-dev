try {
    JSON.parse('{"a":}');
}
catch(e) {
    assertEq(e.lineNumber, 2);
    assertEq(e.columnNumber, 10);
}
