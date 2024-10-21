// |jit-test| --enable-error-iserror

// Check if Error.isError is available
if (typeof Error.isError !== "function") {
    quit();
}

// Test non-object input should return false
assertEq(Error.isError(null), false);
assertEq(Error.isError(undefined), false);
assertEq(Error.isError(123), false);
assertEq(Error.isError("string"), false);

// Test plain objects should return false
assertEq(Error.isError({}), false);
assertEq(Error.isError(new Object()), false);

// Test various error objects should return true
assertEq(Error.isError(new Error()), true);
assertEq(Error.isError(new TypeError()), true);
assertEq(Error.isError(new RangeError()), true);

// Test cross-compartment wrapper (CCW) objects
const g = newGlobal({ newCompartment: true });
const e = g.eval(`new Error()`);
assertEq(Error.isError(e), true);

nukeCCW(e);

// Test Error.isError for the nuked CCW object
let caught = false;
try {
    Error.isError(e);
} catch (ex) {
    caught = true;
    assertEq(ex.message, "can't access dead object");
}
assertEq(caught, true);
