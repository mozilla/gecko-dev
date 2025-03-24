// Test a code range size that's larger than the buffer
let bytes = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 4, 1, 96, 0, 0, 3, 2, 1, 0, 10, 240, 67, 0, 0, 0, 12, 1, 10, 0, 252, 2, 3, 1, 1, 0, 0, 110, 26, 11, 161, 10]);
assertErrorMessage(() => new WebAssembly.Module(bytes), WebAssembly.CompileError, /function body count does not match function signature count/);
assertEq(WebAssembly.validate(bytes), false);
