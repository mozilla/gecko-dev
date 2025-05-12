function test() {
  // Mutating SharedArrayBuffer.prototype.constructor pops the fuse. A no-op change is fine.
  newGlobal().evaluate(`
    assertEq(getFuseState().OptimizeSharedArrayBufferSpeciesFuse.intact, true);
    let p = SharedArrayBuffer.prototype.constructor;
    SharedArrayBuffer.prototype.constructor = p;
    assertEq(getFuseState().OptimizeSharedArrayBufferSpeciesFuse.intact, true);
    SharedArrayBuffer.prototype.constructor = Object;
    assertEq(getFuseState().OptimizeSharedArrayBufferSpeciesFuse.intact, false);
  `);

  // Mutating SharedArrayBuffer[Symbol.species] pops the fuse.
  newGlobal().evaluate(`
    assertEq(getFuseState().OptimizeSharedArrayBufferSpeciesFuse.intact, true);
    delete SharedArrayBuffer[Symbol.species];
    assertEq(getFuseState().OptimizeSharedArrayBufferSpeciesFuse.intact, false);
  `);
}
test();
