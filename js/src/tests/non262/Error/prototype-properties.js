const nativeErrors = [
    EvalError,
    RangeError,
    ReferenceError,
    SyntaxError,
    TypeError,
    URIError
];

const ownKeys = Reflect.ownKeys(Error.prototype);
for (const expected of ["constructor", "message", "name", "toString"]) {
  assertEq(ownKeys.includes(expected), true, "Error.prototype should have a key named " + expected);
}
assertEq(Error.prototype.name, "Error");
assertEq(Error.prototype.message, "");

for (const error of nativeErrors) {
    assertEq(Reflect.ownKeys(error.prototype).sort().toString(), "constructor,message,name");
    assertEq(error.prototype.name, error.name);
    assertEq(error.prototype.message, "");
    assertEq(error.prototype.constructor, error);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
