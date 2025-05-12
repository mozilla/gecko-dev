function test() {
  // Mutating ArrayBuffer.prototype.constructor pops the fuse. A no-op change is fine.
  newGlobal().evaluate(`
    assertEq(getFuseState().OptimizeArrayBufferSpeciesFuse.intact, true);
    let p = ArrayBuffer.prototype.constructor;
    ArrayBuffer.prototype.constructor = p;
    assertEq(getFuseState().OptimizeArrayBufferSpeciesFuse.intact, true);
    ArrayBuffer.prototype.constructor = Object;
    assertEq(getFuseState().OptimizeArrayBufferSpeciesFuse.intact, false);
  `);

  // Mutating ArrayBuffer[Symbol.species] pops the fuse.
  newGlobal().evaluate(`
    assertEq(getFuseState().OptimizeArrayBufferSpeciesFuse.intact, true);
    delete ArrayBuffer[Symbol.species];
    assertEq(getFuseState().OptimizeArrayBufferSpeciesFuse.intact, false);
  `);
}
test();
