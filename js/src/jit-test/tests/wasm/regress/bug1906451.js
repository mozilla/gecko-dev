let { table, func } = wasmEvalText(`(module
  (type (func))
  (func (type 0))
  (table 1 1 (ref 0) ref.func 0)
  (export "table" (table 0))
  (export "func" (func 0))
)`).exports;

// Check that the default value initializer worked
let element0 = table.get(0);
assertEq(element0, func);

// Set the first element to itself using the JS-API
// table[0] = table[0]
table.set(0, element0);

// Check that we didn't null out that value somehow
assertEq(element0, table.get(0));
