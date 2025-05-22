// non-null table initialization
var { get1, get2, get3, get4 } = wasmEvalText(`(module
  (type $dummy (func))
  (func $dummy)

  (table $t1 10 funcref)
  (table $t2 10 funcref (ref.func $dummy))
  (table $t3 10 (ref $dummy) (ref.func $dummy))
  (table $t4 10 (ref func) (ref.func $dummy))

  (func (export "get1") (result funcref) (table.get $t1 (i32.const 1)))
  (func (export "get2") (result funcref) (table.get $t2 (i32.const 4)))
  (func (export "get3") (result funcref) (table.get $t3 (i32.const 7)))
  (func (export "get4") (result funcref) (table.get $t4 (i32.const 5)))
)`).exports;
assertEq(get1(), null);
assertEq(get2() != null, true);
assertEq(get3() != null, true);
assertEq(get4() != null, true);

const sampleWasmFunction = get2();
sampleWasmFunction();

// Invalid initializers
for (let i of [
  `(table $t1 10 (ref $dummy) (ref.func $dummy1))`,
  `(table $t2 5 10 (ref func))`,
  `(table $t3 10 10 (ref func) (ref.null $dummy1))`,
  `(table $t4 10 (ref $dummy))`,
  '(table $t5 1 (ref $dummy) (ref.null $dummy))',
]) {
  wasmFailValidateText(`(module
    (type $dummy (func))
    (type $dummy1 (func (param i32)))
    (func $dummy1 (param i32))

    ${i}
  )`, /(type mismatch|table with non-nullable references requires initializer)/);
}

let { t1, t2 } = new wasmEvalText(`(module
  (func $dummy)
  (table (export "t1") 10 funcref (ref.func $dummy))
  (table (export "t2") 10 (ref func) (ref.func $dummy))
)`).exports;
assertEq(t1.get(2) != null, true);

// Imported tables require strict equality of their reference types.
wasmEvalText(`(module
  (import "" "t1" (table 10 funcref))
  (import "" "t2" (table 10 (ref func)))
)`, { "": { t1, t2 } });
assertErrorMessage(
  () => wasmEvalText(`(module (import "" "t1" (table 10 (ref func))))`, { "": { t1 } }),
  WebAssembly.LinkError, /imported table type mismatch/,
);
assertErrorMessage(
  () => wasmEvalText(`(module (import "" "t2" (table 10 funcref)))`, { "": { t2 } }),
  WebAssembly.LinkError, /imported table type mismatch/,
);

// Tables with non-nullable types require init expressions, but only if they
// are defined in the module.
wasmFailValidateText(`(module
  (table $t 10 (ref func))
)`, /table with non-nullable references requires initializer/);
wasmValidateText(`(module
  (import "" "t1" (table 10 (ref func)))
)`);

wasmFailValidateText(`
(module
  (func $dummy)
  (table (export "t") 10 funcref (ref.null none))
)`, /type mismatch/);

const foo = "bar";
const { t3 } = wasmEvalText(`
(module
  (global (import "" "foo") externref)
  (table (export "t3") 6 20 externref (global.get 0))
)`, { "": { "foo": foo } }).exports;

assertEq(t3.get(5), "bar");
assertThrows(() => { t3.get(7) });
assertThrows(() => { t3.grow(30, null) });
t3.grow(8, {t: "test"});
assertEq(t3.get(3), "bar");
assertEq(t3.get(7).t, "test");

// Fail because tables come before globals in the binary format, so tables
// cannot refer to globals.
wasmFailValidateText(`(module
  (global $g1 externref)
  (table 10 externref (global.get $g1))
)`, /global.get index out of range/);

function assertThrows(f) {
  var ok = false;
  try {
      f();
  } catch (exc) {
      ok = true;
  }
  if (!ok)
      throw new TypeError("Assertion failed: " + f + " did not throw as expected");
}
