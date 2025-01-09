// |reftest| shell-option(--enable-error-iserror) skip-if(!Error.isError)

/*---
features: [Error.isError]
---*/

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

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
