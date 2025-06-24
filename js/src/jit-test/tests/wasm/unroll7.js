// |jit-test| skip-if: true

// Tests that unrolling actually transforms a loop into something new, by
// unrolling a loop with just one FP mul and one FP add in it and checking that
// the resulting assembly contains 4 FP muls and 4 FP adds.  Matching bits of
// text in the disassembly is necessarily platform-dependent, but we don't care
// about that since unrolling is done at the MIR level, so if it works for one
// target, it works for all.
//
// Test inspired by simd/pmaddubsw-x64-ion-codegen.js.

const isX64 = getBuildConfiguration("x64") && !getBuildConfiguration("simulator");

if (hasDisassembler() && isX64) {
  let t = `
  (module
    (memory 1)
    (func (export "dot_product")
          (param $startA i32) (param $startB i32) (param $count i32)
      (local $sum f64)
      (if (i32.gt_u (local.get $count) (i32.const 0))
        (then
          (loop $cont
            (local.set $sum (f64.add (local.get $sum)
              (f64.mul (f64.load (local.get $startA))
                       (f64.load (local.get $startB)))))
            (local.set $startA (i32.add (local.get $startA) (i32.const 8)))
            (local.set $startB (i32.add (local.get $startB) (i32.const 8)))
            (local.set $count (i32.sub (local.get $count) (i32.const 1)))
            (br_if $cont (i32.gt_u (local.get $count) (i32.const 0)))
          )
        )
      )
    )
  )`;

  let i = new WebAssembly.Instance(
             new WebAssembly.Module(wasmTextToBinary(t)));

  const output = wasmDis(i.exports.dot_product, {tier:"ion", asString:true})
                 .replace(/^[0-9a-f]{8}  (?:[0-9a-f]{2} )+\n?\s+/gmi, "");

  // Find four "mulsd .. addsd" bits of text.  One pair for the peeled
  // iteration and 3 pairs for the 3 unrolled iterations.

  const re = /\bv?mulsd[^\n]+\nv?addsd /g;
  assertEq(re.exec(output) != null, true);
  assertEq(re.exec(output) != null, true);
  assertEq(re.exec(output) != null, true);
  assertEq(re.exec(output) != null, true);
  assertEq(re.exec(output) == null, true);
}
