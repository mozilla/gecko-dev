// |jit-test| --setpref=wasm_relaxed_simd=true; skip-if: !wasmRelaxedSimdEnabled()

// Relaxed simd allows its operations to produce implementation-specific
// results as carefully described by the spec.  That's OK; however we require
// that baseline and Ion produce the same results.  Hence what is important
// about this test program is not the results directly, but that a baseline and
// Ion run produce the same results.  Since we have no way to directly test
// that, this test case at least hardwires results and expects them to be the
// same regardless of which compiler is used.
//
// For each `op`, "variant1" uses arguments 4 x 0x0000'0000 and 4 x
// 0xFFFF'FFFF, and "variant2" has the args the other way around.
//
// These tests are necessary because 0xFFFF'FFFF is a negative NaN, and the
// underlying Intel instructions are sensitive to operand ordering in the case
// where the input (lanes) hold NaNs.

for (let op of ["f32x4.relaxed_min", "f32x4.relaxed_max",
                "f64x2.relaxed_min", "f64x2.relaxed_max"]) {
    const t = `
      (module
        (func (export "variant1") (result i32)
          (i32.const 0)  i8x16.splat
          (i32.const 0)  i8x16.splat
          (i32.const 0)  i8x16.splat
          i32x4.eq
          ;; stack top = 4 x 0xFFFF'FFFF ; stack top-1 = 4 x 0x0000'0000
          ` + op + `
          i8x16.bitmask
        )
        (func (export "variant2") (result i32)
          (i32.const 0)  i8x16.splat
          (i32.const 0)  i8x16.splat
          i32x4.eq
          (i32.const 0)  i8x16.splat
          ;; stack top = 4 x 0x0000'0000 ; stack top-1 = 4 x 0xFFFF'FFFF
          ` + op + `
          i8x16.bitmask
        )
      )
    `;

    let i =
        new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(t)));
    // To reiterate comments above, we don't care what `result1` and `result2`
    // are (although it would be very strange if they had any value other than
    // zero or 65535).  We care only that we get the same values on Ion and
    // baseline.
    let result1 = i.exports.variant1();
    let result2 = i.exports.variant2();
    if (getBuildConfiguration("arm64")) {
      // The relaxed_min/max operation appears to propagate NaNs symmetrically
      // from either arg
      assertEq(result1, 65535);
      assertEq(result2, 65535);
    } else {
      // x86_32 or x86_64, presumably.  What happens when one of the args
      // contains a NaN depends on which arg it is.  See Intel documentation on
      // `minps`/`maxps`.
      assertEq(result1, 65535);
      assertEq(result2, 0);
    }
}
