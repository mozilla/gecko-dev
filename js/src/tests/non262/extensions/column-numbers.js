// SKIP test262 export
// Relies on the file name used to run the test.

actual   = 'No Error';
expected = /column-numbers\.js:7:11/;
try {
    throw new Error("test"); // line 7
}
catch(ex) {
    actual = ex.stack;
    print('Caught exception ' + ex.stack);
}
reportMatch(expected, actual, 'column number present');
