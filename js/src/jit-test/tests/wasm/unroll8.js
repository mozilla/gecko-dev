// |jit-test| skip-if: wasmCompileMode() != "ion"

// Check that the unroller properly takes account of dependencies
// through memory.  See bug 1972116.

let t = `
(module
  (type $structTy (struct (field (mut f32))))
  (func (export "badness") (param $struct (ref null $structTy)) (result f32)
    (local $f f32)
    (local $i i32)
    (block $break
      (loop $cont
        ;; f = struct->field0
        (local.set $f (struct.get $structTy 0 (local.get $struct)))
        ;; f = f + 360.0
        (local.set $f (f32.add (local.get $f) (f32.const 360.0)))
        ;; struct->field0 = f
        (struct.set $structTy 0 (local.get $struct) (local.get $f))
        ;; loop control
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br_if $cont (i32.lt_u (local.get $i) (i32.const 12345)))
      )
    )
    (struct.get $structTy 0 (local.get $struct))
  )

  (func (export "go") (result f32)
    (struct.new_default $structTy)
    (call 0)
  )
)`;

let i = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(t)));

// The correct result is 4444200.  If the loop is unrolled and peeled without
// regard to dependencies through memory, and then GVNd, an observed incorrect
// result is 360, because the struct.gets in the unrolled iterations end up
// being GVNd out and replaced by the value of 0.0 returned by the struct.get
// in the peeled iteration.
assertEq(i.exports.go(), 4444200);
