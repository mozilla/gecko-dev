assertThrowsInstanceOfWithMessage(
    () => eval("for (let case of ['foo', 'bar']) {}"),
    SyntaxError,
    "unexpected token: keyword 'case'");

assertThrowsInstanceOfWithMessage(
    () => eval("for (let debugger of ['foo', 'bar']) {}"),
    SyntaxError,
    "unexpected token: keyword 'debugger'");

assertThrowsInstanceOfWithMessage(
    () => eval("for (let case in ['foo', 'bar']) {}"),
    SyntaxError,
    "unexpected token: keyword 'case'");

assertThrowsInstanceOfWithMessage(
    () => eval("for (let debugger in ['foo', 'bar']) {}"),
    SyntaxError,
    "unexpected token: keyword 'debugger'");

if (typeof reportCompare === "function")
    reportCompare(0, 0);

