// |jit-test| skip-if: !wasmSimdEnabled()

const m = new WebAssembly.Module(wasmTextToBinary(`(module
  (import "imp" "f" (func $f (result i32)))

  (type $node (struct
    (field $v (mut (ref null $node)))
  ))

  (memory (export "mem") 1 1)

  (func (export "run")
    (v128.store (i32.const 0) (
      call $doit (
        v128.load (i32.const 0)
        v128.load (i32.const 0)
        v128.load (i32.const 0)
        v128.load (i32.const 0)
        v128.load (i32.const 0)
        v128.load (i32.const 0)
        v128.load (i32.const 0)
        v128.load (i32.const 0)
      )
    ))
  )

  (func $doit
    ;; Enough Simd128 parameters and locals to ensure registers v8-v15 are used.
    (param v128) (param v128) (param v128) (param v128)
    (param v128) (param v128) (param v128) (param v128)

    (result v128)

    (local $var1 v128) (local $var2 v128) (local $var3 v128) (local $var4 v128)
    (local $var5 v128) (local $var6 v128) (local $var7 v128) (local $var8 v128)

    (local $var_s (ref $node))
    (local $var_c (ref $node))

    (struct.new_default $node)
    local.set $var_s

    ;; GC to tenure $var_s
    (drop (call $f))

    ;; New nursery allocated $var_c
    (struct.new_default $node)
    local.set $var_c

    (v128.const i32x4 0x1111_1111 0x1111_2222 0x1111_3333 0x1111_4444)
    local.set $var1

    (v128.const i32x4 0x2222_1111 0x2222_2222 0x2222_3333 0x2222_4444)
    local.set $var2

    (v128.const i32x4 0x3333_1111 0x3333_2222 0x3333_3333 0x3333_4444)
    local.set $var3

    (v128.const i32x4 0x4444_1111 0x4444_2222 0x4444_3333 0x4444_4444)
    local.set $var4

    (v128.const i32x4 0x5555_1111 0x5555_2222 0x5555_3333 0x5555_4444)
    local.set $var5

    (v128.const i32x4 0x6666_1111 0x6666_2222 0x6666_3333 0x6666_4444)
    local.set $var6

    (v128.const i32x4 0x7777_1111 0x7777_2222 0x7777_3333 0x7777_4444)
    local.set $var7

    (v128.const i32x4 0x8888_1111 0x8888_2222 0x8888_3333 0x8888_4444)
    local.set $var8

    local.get $var1
    local.get 0
    v128.or

    local.get $var2
    local.get 1
    v128.or

    local.get $var3
    local.get 2
    v128.or

    local.get $var4
    local.get 3
    v128.or

    local.get $var5
    local.get 4
    v128.or

    local.get $var6
    local.get 5
    v128.or

    local.get $var7
    local.get 6
    v128.or

    local.get $var8
    local.get 7
    v128.or

    local.get $var1
    local.get 1
    v128.or

    local.get $var2
    local.get 1
    v128.or

    local.get $var3
    local.get 1
    v128.or

    local.get $var4
    local.get 1
    v128.or

    local.get $var5
    local.get 1
    v128.or

    local.get $var6
    local.get 1
    v128.or

    local.get $var7
    local.get 1
    v128.or

    local.get $var8
    local.get 1
    v128.or

    ;; Postwrite barrier with ABI call.
    (struct.set $node 0 (local.get $var_s) (local.get $var_c))

    v128.or
    v128.or
    v128.or
    v128.or
    v128.or
    v128.or
    v128.or

    v128.or
    v128.or
    v128.or
    v128.or
    v128.or
    v128.or
    v128.or

    v128.or
  )
)`));

const {
  run, mem
} = new WebAssembly.Instance(m, {
  imp: {
    f() {
      gc();
      return 0;
    }
  }
}).exports;

var view = new BigUint64Array(mem.buffer, 0, 2);

for (let i = 0; i < 100; ++i) {
  view[0] = 0n;
  view[1] = 0n;

  run();

  assertEq(view[0].toString(16), "ffff2222ffff1111");
  assertEq(view[1].toString(16), "ffff4444ffff3333");
}
