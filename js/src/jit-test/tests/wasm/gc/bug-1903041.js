// |jit-test| test-also=--setpref=wasm_test_serialization=true

wasmFailValidateText(`(module
  (type $a (struct))
  (rec
    (type $b (sub (struct
      (field (ref $b))
    )))
    (type $c (sub $b (struct
      (field (ref $a))
    )))
  )
)`, /incompatible super type/);

wasmFailValidateText(`(module
  (type $a (struct))
  (rec
    (type $b (sub (struct
      (field (ref $b))
    )))
    (type $c (sub $b (struct
      (field (ref $c))
    )))
    (type $d (sub $c (struct
      (field (ref $a))
    )))
  )
)`, /incompatible super type/);
