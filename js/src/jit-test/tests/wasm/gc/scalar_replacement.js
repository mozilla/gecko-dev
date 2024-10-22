// |jit-test| skip-if: !wasmGcEnabled()

// Scalar Replacement of wasm structures.

// A very simple example, this struct should be replaced.
var code =`
(module
  (type $point (struct
    (field $x i32)
    (field $y i32)
  ))

  (func $test (result i32)
    i32.const 42
    i32.const 11
    struct.new $point
    struct.get $point 1
    return
  )
)`;

let module = new WebAssembly.Module(wasmTextToBinary(code));
instance = new WebAssembly.Instance(module);

// This struct contains a lot of fields and should not be optimized.
code =`
(module
  (type $point (struct
    (field $x i32)
    (field $y1 i32)
    (field $x2 i32)
    (field $y3 i32)
    (field $x4 i32)
    (field $y5 i32)
    (field $x6 i32)
    (field $y7 i32)
    (field $x8 i32)
    (field $y9 i32)
    (field $x10 i32)
    (field $y11 i32)
    (field $x12 i32)
    (field $y13 i32)
    (field $x14 i32)
    (field $y15 i32)
    ))

  (func $test (result i32)
    i32.const 42
    i32.const 42
    i32.const 42
    i32.const 42
    i32.const 42
    i32.const 42
    i32.const 42
    i32.const 42
    i32.const 42
    i32.const 42
    i32.const 42
    i32.const 42
    i32.const 42
    i32.const 42
    i32.const 42
    i32.const 42
    struct.new $point
    struct.get $point 10
    return
  )
)`;

module = new WebAssembly.Module(wasmTextToBinary(code));

// Using parameters instead of constants to create the struct.
code =`
(module
  (type $point (struct
    (field $x i32)
    (field $y i32)
  ))

  (func $test (param i32) (param i32) (result i32)
    local.get 0
    local.get 1
    struct.new $point
    struct.get $point 1
    return
  )
)`;

module = new WebAssembly.Module(wasmTextToBinary(code));
instance = new WebAssembly.Instance(module);


// Filling the struct with if/then/else structure around it.
code =`
(module
    (type $point (struct
    (field $x (mut i32))
    (field $y (mut i32))
    ))

    (func $createAndReturnField (param $cond i32) (result i32)
    (local $s (ref null $point))

    ;; Initialize the struct with some constants
    (struct.new $point (i32.const 12) (i32.const 13))
    (local.set $s)

    (local.get $cond)
    (if (then
        (struct.set $point $x (local.get $s) (i32.const 10))
        (struct.set $point $y (local.get $s) (i32.const 20))
    ) (else
        (struct.set $point $x (local.get $s) (i32.const 30))
        (struct.set $point $y (local.get $s) (i32.const 40))
    ))

    (struct.get $point $x (local.get $s))
    )
)`;

module = new WebAssembly.Module(wasmTextToBinary(code));
instance = new WebAssembly.Instance(module);


// Same test but with a struct escaping in one of the branches.
// This should prevent the struct to be optimized.
code =`
(module
    (type $point (struct
    (field $x (mut i32))
    (field $y (mut i32))
    ))

    (global $escapedPoint (mut (ref null $point)) (ref.null $point))

    (func $createAndReturnField (param $cond i32) (result i32)
    (local $s (ref null $point))

    ;; Initialize the struct with some constants
    (struct.new $point (i32.const 12) (i32.const 13))
    (local.set $s)

    (local.get $cond)
    (if (then
        (struct.set $point $x (local.get $s) (i32.const 10))
        (struct.set $point $y (local.get $s) (i32.const 20))
        ;; Storing this struct in a global
        (global.set $escapedPoint (local.get $s))
    ) (else
        (struct.set $point $x (local.get $s) (i32.const 30))
        (struct.set $point $y (local.get $s) (i32.const 40))
    ))

    (struct.get $point $x (local.get $s))
    )
)`;

module = new WebAssembly.Module(wasmTextToBinary(code));
instance = new WebAssembly.Instance(module);
