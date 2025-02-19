// |jit-test| skip-if: !getPrefValue("wasm_lazy_tiering");

const m1 = new WebAssembly.Module(wasmTextToBinary(`(module
  (type (struct))
  (type $f (func))
  (type (func (param i32 i64) (result i32)))
  (type (array (mut i8)))
  (type $s (sub (struct)))
  (type (sub final (struct (field i32))))
  (type (sub final $s (struct (field i32))))
  (type (array (mut i8)))
  (rec
    (type (struct (field i8) (field (mut i16)) (field (mut i32))))
    (type (array (mut i8)))
  )
  (type (struct (field anyref)))
  (type (struct (field (ref any))))
  (type (struct (field (ref null none))))
  (type (struct (field (ref 3))))
  (type (struct (field (ref null 3))))

  (import "foo" "bar" (func (type $f)))
  (import "foo" "💇🏻‍♂️" (func (param i32) (result anyref)))
  (import "foo" "\\"big cheese\\"" (func (type $f)))

  (import "foo" "table32" (table i32 1 2 funcref))
  (import "foo" "table64" (table i64 1 funcref))

  (import "foo" "memory32" (memory 1 2))
  (import "foo" "memory64" (memory i64 1))

  (import "foo" "globalStruct" (global (mut structref)))
  (import "foo" "globalI64" (global i64))

  (import "foo" "tag1" (tag))
  (import "foo" "tag2" (tag (param i32 i64)))

  (memory 1 2)
  (memory (export "memory") i64 1)

  (table $tab 10 100 funcref)
  (table (export "table") i64 10 anyref (ref.i31 (i32.const 123)))
  (table 10 funcref (ref.null $f))
  (table 10 funcref (ref.null func))

  (global (ref eq) (struct.new $s))
  (global (export "global") (mut i32) (i32.const 0))
  (global i32 i32.const 2 i32.const 1 i32.sub)
  (global (ref i31) (ref.i31 (i32.const 123)))

  (tag)
  (tag (export "tag") (param i32))

  (export "foo" (table 2))

  (start 0)

  (elem func 1 0 1 0)
  (elem (ref func) (ref.func 1) (ref.func 0) (ref.func 1) (ref.func 0))
  (elem (ref func) (ref.func 1))
  (elem anyref (ref.i31 (i32.const 123)) (ref.null any))
  (elem (table $tab) (offset i32.const 0) func 1 0 1 0)
  (elem (table $tab) (offset i32.const 0) funcref (ref.func 1))
  (elem (table $tab) (offset (i32.add (i32.const 2) (i32.const 3))) func 1 0 1 0)
  (elem (table $tab) (offset (i32.add (i32.const 2) (i32.const 3))) funcref (ref.func 1) (ref.func 0))
  (elem (table $tab) (offset i32.const 2) funcref (ref.func 1) (ref.func 0))
  (elem declare funcref (ref.func 0))
  (elem declare funcref (ref.func 1) (ref.func 0))
  (elem (table $tab) (offset (i32.add (i32.const 2) (i32.const 3))) funcref (ref.func 1))

  (func unreachable)
  (func (export "😎") (param i32 i32) (result i64)
    (local i32)

    local.get 0
    local.get 1
    i32.add
    local.set 2

    local.get 2
    i64.extend_i32_u
  )
  (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32))
  (func
    (local i32 i64)

    (if (i32.const 0)
      (then unreachable)
      (else unreachable)
    )
  )

  (data "asdfasdf")
  (data (memory 0) (offset i32.const 0) "asdfasdf")
  (data (memory 1) (offset i64.const 0) "asdfasdf\\00")
  (data (memory 0) (offset (i32.add (i32.const 2) (i32.const 3))) "asdfasdf")
  (data "The \\"big cheese\\" needs...\\nhis cut. 💇🏻‍♂️")
)`));
const t1 = wasmModuleToText(m1);
print(t1);

const m2 = new WebAssembly.Module(wasmTextToBinary(t1));
const t2 = wasmModuleToText(m2);
assertEq(t1, t2);

const imports = {
  "foo": {
    "bar": () => {},
    "💇🏻‍♂️": () => {},
    "\"big cheese\"": () => {},

    "table32": new WebAssembly.Table({ element: "anyfunc", initial: 1, maximum: 2 }),
    "table64": new WebAssembly.Table({ element: "anyfunc", address: "i64", initial: 1n }),

    "memory32": new WebAssembly.Memory({ initial: 1, maximum: 2 }),
    "memory64": new WebAssembly.Memory({ address: "i64", initial: 1n }),

    "globalStruct": new WebAssembly.Global({ value: "structref", mutable: true }),
    "globalI64": new WebAssembly.Global({ value: "i64" }),

    "tag1": new WebAssembly.Tag({ parameters: [] }),
    "tag2": new WebAssembly.Tag({ parameters: ["i32", "i64"] }),
  },
};

const { "😎": test1 } = new WebAssembly.Instance(m1, imports).exports;
const { "😎": test2 } = new WebAssembly.Instance(m2, imports).exports;

assertEq(test1(2, 3), 5n);
assertEq(test2(2, 3), 5n);
