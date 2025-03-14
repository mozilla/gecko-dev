// Mutating Promise.prototype.constructor pops the fuse. A no-op change is fine.
newGlobal().evaluate(`
  assertEq(getFuseState().OptimizePromiseLookupFuse.intact, true);
  let v = Promise.prototype.constructor;
  Promise.prototype.constructor = v;
  assertEq(getFuseState().OptimizePromiseLookupFuse.intact, true);
  Promise.prototype.constructor = {};
  assertEq(getFuseState().OptimizePromiseLookupFuse.intact, false);
`);

// Same for Promise.prototype.then.
newGlobal().evaluate(`
  assertEq(getFuseState().OptimizePromiseLookupFuse.intact, true);
  let v = Promise.prototype.then;
  Promise.prototype.then = v;
  assertEq(getFuseState().OptimizePromiseLookupFuse.intact, true);
  Promise.prototype.then = x => x;
  assertEq(getFuseState().OptimizePromiseLookupFuse.intact, false);
`);

// Same for Promise.resolve.
newGlobal().evaluate(`
  assertEq(getFuseState().OptimizePromiseLookupFuse.intact, true);
  let v = Promise.resolve;
  Promise.resolve = v;
  assertEq(getFuseState().OptimizePromiseLookupFuse.intact, true);
  delete Promise.resolve;
  assertEq(getFuseState().OptimizePromiseLookupFuse.intact, false);
`);

// Same for the Promise[@@species] getter.
newGlobal().evaluate(`
  assertEq(getFuseState().OptimizePromiseLookupFuse.intact, true);
  Object.defineProperty(Promise, Symbol.species, {});
  assertEq(getFuseState().OptimizePromiseLookupFuse.intact, true);
  Object.defineProperty(Promise, Symbol.species, {value: null});
  assertEq(getFuseState().OptimizePromiseLookupFuse.intact, false);
`);
