wasmEvalText(`(module
  (type $a (struct (field anyref)))
  (type $b (struct (field externref)))

  (func
    ref.null $a
    struct.get $a 0
    drop

    ref.null $b
    struct.get $b 0
    drop
  )
)`);
