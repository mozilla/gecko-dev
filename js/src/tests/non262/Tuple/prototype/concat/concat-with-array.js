// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

// SKIP test262 export
// Array.prototype.concat currently doesn't support spreading tuples. The spec
// for records and tuples is undergoing major (design) changes, so disable the
// test for now.

assertDeepEq([1, 2].concat(#[3, 4]), [1, 2, 3, 4]);
assertDeepEq([].concat(#[3, 4]), [3, 4]);
assertDeepEq([].concat(#[]), []);
assertDeepEq([1, 2, 3].concat(#[]), [1, 2, 3]);

reportCompare(0, 0);
