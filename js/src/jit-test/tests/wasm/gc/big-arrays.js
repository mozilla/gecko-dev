// |jit-test| slow; allow-oom; skip-if: getBuildConfiguration("simulator")

//////////////////////////////////////////////////////////////////////////////
//
// Checks for requests for oversize arrays (more than MaxArrayPayloadBytes),
// where MaxArrayPayloadBytes == 1,987,654,321.

const MaxArrayPayloadBytes = 1987654321;

function maxNumElementsForSize(bytes, elemSize) {
  let n = bytes;
  n = bytes & ~0b111; // round down to nearest multiple of gc::CellAlignSize
  n -= getBuildConfiguration()['pointer-byte-size']; // subtract size of data header
  n = Math.floor(n / elemSize); // divide by elemSize and round down
  return n;
}

// // Test code for maxNumElementsForSize
// for (const bytes of [32, 33, 112]) {
//   for (const size of [1, 2, 4, 8, 16]) {
//     print(bytes, size, "->", maxNumElementsForSize(bytes, size));
//   }
// }
// throw "see above";

// array.new
{
  const { test1, test2, test4, test8, test16 } = wasmEvalText(`(module
    (type $1 (array i8))
    (type $2 (array i16))
    (type $4 (array i32))
    (type $8 (array i64))
    ${wasmSimdEnabled() ? `
      (type $16 (array v128))
    ` : ""}
    (func (export "test1") (param i32) (result eqref)
      (array.new $1 (i32.const 0xAB) (local.get 0))
    )
    (func (export "test2") (param i32) (result eqref)
      (array.new $2 (i32.const 0xCD) (local.get 0))
    )
    (func (export "test4") (param i32) (result eqref)
      (array.new $4 (i32.const 0xABCD1234) (local.get 0))
    )
    (func (export "test8") (param i32) (result eqref)
      (array.new $8 (i64.const 0xABCD1234) (local.get 0))
    )
    ${wasmSimdEnabled() ? `
      (func (export "test16") (param i32) (result eqref)
        (array.new $16 (v128.const i64x2 0xABCD1234 0xABCD1234) (local.get 0))
      )
    ` : ""}
  )`).exports;

  const tests = [[1, test1], [2, test2], [4, test4], [8, test8]];
  if (wasmSimdEnabled()) {
    tests.push([16, test16]);
  }
  for (const [size, test] of tests) {
    for (const doMasm of [false, true]) {
      if (doMasm) {
        // Prime the alloc site with a successful allocation so we hit masm from here on out
        test(0);
      }

      // Test boundaries of implementation limit
      const maxNumElements = maxNumElementsForSize(MaxArrayPayloadBytes, size);
      test(maxNumElements);
      assertErrorMessage(() => test(maxNumElements + 1), WebAssembly.RuntimeError, /too many array elements/);

      // Test around signed overflow boundary
      for (let i = -16; i <= 16; i++) {
        assertErrorMessage(
          () => test(maxNumElementsForSize(Math.pow(2, 31) + i, size)),
          WebAssembly.RuntimeError,
          /too many array elements/,
        );
      }

      // Test around unsigned overflow boundary
      for (let i = -16; i <= -1; i++) {
        assertErrorMessage(() => test(i), WebAssembly.RuntimeError, /too many array elements/);
      }
    }
  }
}

// array.new_default
{
  const { test1, test2, test4, test8, test16 } = wasmEvalText(`(module
    (type $1 (array i8))
    (type $2 (array i16))
    (type $4 (array i32))
    (type $8 (array i64))
    ${wasmSimdEnabled() ? `
      (type $16 (array v128))
    ` : ""}
    (func (export "test1") (param i32) (result eqref)
      (array.new_default $1 (local.get 0))
    )
    (func (export "test2") (param i32) (result eqref)
      (array.new_default $2 (local.get 0))
    )
    (func (export "test4") (param i32) (result eqref)
      (array.new_default $4 (local.get 0))
    )
    (func (export "test8") (param i32) (result eqref)
      (array.new_default $8 (local.get 0))
    )
    ${wasmSimdEnabled() ? `
      (func (export "test16") (param i32) (result eqref)
        (array.new_default $16 (local.get 0))
      )
    ` : ""}
  )`).exports;

  const tests = [[1, test1], [2, test2], [4, test4], [8, test8]];
  if (wasmSimdEnabled()) {
    tests.push([16, test16]);
  }
  for (const [size, test] of tests) {
    for (const doMasm of [false, true]) {
      if (doMasm) {
        // Prime the alloc site with a successful allocation so we hit masm from here on out
        test(0);
      }

      // Test boundaries of implementation limit
      const maxNumElements = maxNumElementsForSize(MaxArrayPayloadBytes, size);
      test(maxNumElements);
      assertErrorMessage(() => test(maxNumElements + 1), WebAssembly.RuntimeError, /too many array elements/);

      // Test around signed overflow boundary
      for (let i = -16; i <= 16; i++) {
        assertErrorMessage(
          () => test(maxNumElementsForSize(Math.pow(2, 31) + i, size)),
          WebAssembly.RuntimeError,
          /too many array elements/,
        );
      }

      // Test around unsigned overflow boundary
      for (let i = -16; i <= -1; i++) {
        assertErrorMessage(() => test(i), WebAssembly.RuntimeError, /too many array elements/);
      }
    }
  }
}

// array.new_fixed
{
  assertErrorMessage(() => wasmEvalText(`(module
    (type $a (array i8))
    (func (result (ref $a))
      array.new_fixed $a 10001
    )
  )`), WebAssembly.CompileError, /too many array.new_fixed elements/);
  assertErrorMessage(() => wasmEvalText(`(module
    (type $a (array f32))
    (func (result (ref $a))
      array.new_fixed $a 10001
    )
  )`), WebAssembly.CompileError, /too many array.new_fixed elements/);
  assertErrorMessage(() => wasmEvalText(`(module
    (type $a (array f32))
    (func (result (ref $a))
      array.new_fixed $a 2147483647
    )
  )`), WebAssembly.CompileError, /too many array.new_fixed elements/);
  assertErrorMessage(() => wasmEvalText(`(module
    (type $a (array i8))
    (func (result (ref $a))
      array.new_fixed $a 4294967295
    )
  )`), WebAssembly.CompileError, /too many array.new_fixed elements/);
}

// array.new (constant length)
{
  const { testA, testB, testC, testD } = wasmEvalText(`(module
    (type $a (array f32))
    (type $b (array i8))

    (func (export "testA") (result (ref $a))
      (array.new $a (f32.const 0) (i32.const 2147483647))
    )
    (func (export "testB") (result (ref $b))
      ;; overflows due to size of data header
      (array.new $b (i32.const 0) (i32.const 4294967295))
    )
    (func (export "testC") (result (ref $a))
      (array.new $a (f32.const 0) (i32.const -1))
    )
    (func (export "testD") (result (ref $b))
      (array.new $b (i32.const 0) (i32.const -1))
    )
  )`).exports;
  assertErrorMessage(() => testA(), WebAssembly.RuntimeError, /too many array elements/);
  assertErrorMessage(() => testB(), WebAssembly.RuntimeError, /too many array elements/);
  assertErrorMessage(() => testC(), WebAssembly.RuntimeError, /too many array elements/);
  assertErrorMessage(() => testD(), WebAssembly.RuntimeError, /too many array elements/);
}

// array.new_data
// Impossible to test because the max data segment length is 1GB
// (1,073,741,824 bytes) (MaxDataSegmentLengthPages * PageSize), which is less
// than MaxArrayPayloadBytes.

// array.new_element
// Similarly, impossible to test because an element segment can contain at
// most 10,000,000 (MaxElemSegmentLength) entries.
