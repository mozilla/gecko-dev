let defaultableTypes = [
  ['i32', 'i32.const 0 i32.eq'],
  ['i64', 'i64.const 0 i64.eq'],
  ['f32', 'f32.const 0 f32.eq'],
  ['f64', 'f64.const 0 f64.eq'],
  ['anyref', 'ref.is_null'],
  ['funcref', 'ref.is_null'],
  ['externref', 'ref.is_null'],
  ['(ref null $type)', 'ref.is_null'],
];
if (wasmSimdEnabled()) {
  defaultableTypes.push(['v128', 'v128.const i64x2 0 0 i8x16.eq v128.any_true']);
}

for (let [type, isDefault] of defaultableTypes) {
  let {testStruct, testArray} = wasmEvalText(`(module
    (type $type (struct))
    (type $struct (struct (field ${type})))
    (type $array (array ${type}))

    (func (export "testStruct") (result i32)
      (struct.get $struct 0
            struct.new_default $struct)
      ${isDefault}
    )
    (func (export "testArray") (result i32)
      (array.get $array
        (array.new_default $array i32.const 1)
        i32.const 0
      )
      ${isDefault}
    )
  )`).exports;

  assertEq(testStruct(), 1);
  assertEq(testArray(), 1);
}

let nonDefaultableTypes = ['(ref any)', '(ref func)', '(ref extern)', '(ref $type)'];
for (let type of nonDefaultableTypes) {
  wasmFailValidateText(`(module
    (type $type (struct))
    (type $struct (struct (field ${type})))
    (func
      struct.new_default $struct
      drop
    )
  )`, /defaultable/);
  wasmFailValidateText(`(module
    (type $type (struct))
    (type $array (array ${type}))
    (func
      i32.const 1
      array.new_default $array
      drop
    )
  )`, /defaultable/);
}
