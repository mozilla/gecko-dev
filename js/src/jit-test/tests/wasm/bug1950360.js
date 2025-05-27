function wasmEvalText(str, imports, compileOptions) {
  var binary = wasmTextToBinary(str);
  var m = new WebAssembly.Module(binary, compileOptions);
  return new WebAssembly.Instance(m, imports);
}
var ins0 = wasmEvalText(`(module
    (func \$fac-acc (export "fac-acc") (param i64 i64) (result i64)
      (if (result i64) (i64.eqz (local.get 0))
        (then (local.get 1))
        (else
          (return_call \$fac-acc
            (i64.sub (local.get 0) (i64.const 1))
            (i64.mul (local.get 0) (local.get 1))
          )
        )
      )
    )
  )`);
var ins1 = wasmEvalText(`(module
    (import "" "fac-acc" (func \$fac-acc (param i64 i64) (result i64)))
    (type \$ty (func (param i64 i64) (result i64)))
    (table \$t 1 1 funcref)
    (func \$f (export "fac") (param i64) (result i64)
      local.get 0
      i64.const 1
      i32.const 0
      return_call_indirect \$t (type \$ty)
    )
    (elem \$t (i32.const 0) \$fac-acc)
  )`, {"": {"fac-acc": ins0.exports["fac-acc"]}});
const check = () => ins1.exports.fac(true);
for (var i = 0; i < 100; i++) {
  oomTest(check);
}
