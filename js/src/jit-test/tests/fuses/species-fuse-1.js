function test() {
  // Mutating Array.prototype.constructor pops the fuse. A no-op change is fine.
  newGlobal().evaluate(`
    assertEq(getFuseState().OptimizeArraySpeciesFuse.intact, true);
    let p = Array.prototype.constructor;
    Array.prototype.constructor = p;
    assertEq(getFuseState().OptimizeArraySpeciesFuse.intact, true);
    Array.prototype.constructor = Object;
    assertEq(getFuseState().OptimizeArraySpeciesFuse.intact, false);
  `);

  // Mutating Array[Symbol.species] pops the fuse.
  newGlobal().evaluate(`
    assertEq(getFuseState().OptimizeArraySpeciesFuse.intact, true);
    delete Array[Symbol.species];
    assertEq(getFuseState().OptimizeArraySpeciesFuse.intact, false);
  `);

  assertEq(getUseCounterResults().OptimizeArraySpeciesFuse, 2);
}
test();
