// |jit-test| skip-if: !wasmGcEnabled()

// For the time being, we do not want to expose struct types outside of the
// module where they are defined, and so there are restrictions on how functions
// and global variables that traffic in struct types can be used.
//
// At the same time, intra-module uses of functions and globals that use struct
// types must be allowed to the greatest extent possible.
//
// Terminology: A function that takes a Ref parameter or returns a Ref result is
// "exposed for Ref", as is a global of Ref type.  Anyref is OK though, in all
// cases.
//
// To keep it simple we have the following restrictions that can all be checked
// statically.
//
//  - Exported and imported functions cannot be exposed for Ref.
//
//  - If the module has an exported or imported table then no function stored in
//    that table by the module (by means of an element segment) can be exposed
//    for Ref.
//
//  - If the module has an exported or imported table then no call_indirect via
//    that table may reference a type that is exposed for Ref.
//
//  - An exported or imported global cannot be exposed for Ref.
//
// Conversely,
//
//  - If a module has a private table then it can contain private functions that
//    are exposed for Ref and it is possible to call those functions via that
//    table.
//
//  - If a module has a private global then it can be exposed for ref.
//
// Note that
//
//  - code generators can work around the restrictions by instead using
//    functions and globals that use anyref, and by using downcasts to check
//    that the types are indeed correct.  (Though the meaning of downcast will
//    change as the GC feature evolves.)
//
//  - we could probably make the restrictions slightly softer but there's really
//    no advantage to doing so.

function wasmCompile(text) {
    return new WebAssembly.Module(wasmTextToBinary(text));
}

// Exported function can't take ref type parameter, but anyref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $x i32)))
      (func (export "f") (param (ref $box)) (unreachable)))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (func (export "f") (param anyref) (unreachable)))`),
         "object");

// Exported function can't return ref result, but anyref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $x i32)))
      (func (export "f") (result (ref $box)) (ref.null)))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (func (export "f") (result anyref) (ref.null)))`),
         "object");

// Imported function can't take ref parameter, but anyref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $x i32)))
      (import "m" "f" (param (ref $box))))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (import "m" "f" (param anyref)))`),
         "object");

// Imported function can't return ref type, but anyref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $x i32)))
      (import "m" "f" (param i32) (result (ref $box))))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (import "m" "f" (param i32) (result anyref)))`),
         "object");

// Imported global can't be of Ref type (irrespective of mutability), though anyref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $val i32)))
      (import "m" "g" (global (mut (ref $box)))))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $val i32)))
      (import "m" "g" (global (ref $box))))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (import "m" "g" (global (mut anyref))))`),
         "object");

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (import "m" "g" (global anyref)))`),
         "object");

// Exported global can't be of Ref type (irrespective of mutability), though anyref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $val i32)))
      (global $boxg (export "box") (mut (ref $box)) (ref.null)))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $val i32)))
      (global $boxg (export "box") (ref $box) (ref.null)))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (global $boxg (export "box") (mut anyref) (ref.null)))`),
         "object");

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (global $boxg (export "box") anyref (ref.null)))`),
         "object");

// Exported table cannot reference functions that are exposed for Ref, but anyref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $val i32)))
      (table (export "tbl") 1 anyfunc)
      (elem (i32.const 0) $f1)
      (func $f1 (param (ref $box)) (unreachable)))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $val i32)))
      (table (export "tbl") 1 anyfunc)
      (elem (i32.const 0) $f1)
      (func $f1 (result (ref $box)) (ref.null)))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (table (export "tbl") 1 anyfunc)
      (elem (i32.const 0) $f1)
      (func $f1 (param anyref) (unreachable)))`),
         "object");

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (table (export "tbl") 1 anyfunc)
      (elem (i32.const 0) $f1)
      (func $f1 (result anyref) (ref.null)))`),
         "object");

// Imported table cannot reference functions that are exposed for Ref, though anyref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $val i32)))
      (import "m" "tbl" (table 1 anyfunc))
      (elem (i32.const 0) $f1)
      (func $f1 (param (ref $box)) (unreachable)))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $val i32)))
      (import "m" "tbl" (table 1 anyfunc))
      (elem (i32.const 0) $f1)
      (func $f1 (result (ref $box)) (ref.null)))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (import "m" "tbl" (table 1 anyfunc))
      (elem (i32.const 0) $f1)
      (func $f1 (param anyref) (unreachable)))`),
         "object");

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (import "m" "tbl" (table 1 anyfunc))
      (elem (i32.const 0) $f1)
      (func $f1 (result anyref) (ref.null)))`),
         "object");

// Can't call via exported table with type that is exposed for Ref, though anyref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $val i32)))
      (type $fn (func (param (ref $box))))
      (table (export "tbl") 1 anyfunc)
      (func (param i32)
       (call_indirect $fn (ref.null) (get_local 0))))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $val i32)))
      (type $fn (func (result (ref $box))))
      (table (export "tbl") 1 anyfunc)
      (func (param i32) (result (ref $box))
       (call_indirect $fn (get_local 0))))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $fn (func (param anyref)))
      (table (export "tbl") 1 anyfunc)
      (func (param i32)
       (call_indirect $fn (ref.null) (get_local 0))))`),
         "object");

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $fn (func (result anyref)))
      (table (export "tbl") 1 anyfunc)
      (func (param i32) (result anyref)
       (call_indirect $fn (get_local 0))))`),
         "object");

// Can't call via imported table with type that is exposed for Ref, though anyref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $val i32)))
      (type $fn (func (param (ref $box))))
      (import "m" "tbl" (table 1 anyfunc))
      (func (param i32)
       (call_indirect $fn (ref.null) (get_local 0))))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertErrorMessage(() => wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $box (struct (field $val i32)))
      (type $fn (func (result (ref $box))))
      (import "m" "tbl" (table 1 anyfunc))
      (func (param i32) (result (ref $box))
       (call_indirect $fn (get_local 0))))`),
                   WebAssembly.CompileError,
                   /cannot expose reference type/);

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $fn (func (param anyref)))
      (import "m" "tbl" (table 1 anyfunc))
      (func (param i32)
       (call_indirect $fn (ref.null) (get_local 0))))`),
         "object");

assertEq(typeof wasmCompile(
    `(module
      (gc_feature_opt_in 2)
      (type $fn (func (result anyref)))
      (import "m" "tbl" (table 1 anyfunc))
      (func (param i32) (result anyref)
       (call_indirect $fn (get_local 0))))`),
         "object");

// We can call via a private table with a type that is exposed for Ref.

{
    let m = wasmCompile(
        `(module
          (gc_feature_opt_in 2)
          (type $box (struct (field $val i32)))
          (type $fn (func (param (ref $box)) (result i32)))
          (table 1 anyfunc)
          (elem (i32.const 0) $f1)
          (func $f1 (param (ref $box)) (result i32) (i32.const 37))
          (func (export "f") (param i32) (result i32)
           (call_indirect $fn (ref.null) (get_local 0))))`);
    let i = new WebAssembly.Instance(m).exports;
    assertEq(i.f(0), 37);
}
