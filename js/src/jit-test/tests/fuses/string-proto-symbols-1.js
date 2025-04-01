// Adding certain symbol properties to String.prototype or Object.prototype pops
// the fuse.
for (let holder of ["String.prototype", "Object.prototype"]) {
  for (let sym of ["Symbol.match", "Symbol.replace", "Symbol.search", "Symbol.split"]) {
    newGlobal().evaluate(`
      assertEq(getFuseState().OptimizeStringPrototypeSymbolsFuse.intact, true);
      ${holder}[${sym}] = null;
      assertEq(getFuseState().OptimizeStringPrototypeSymbolsFuse.intact, false);
    `);  
  }
}

// Changing String.prototype's proto also pops the fuse. A no-op change is fine.
newGlobal().evaluate(`
  assertEq(getFuseState().OptimizeStringPrototypeSymbolsFuse.intact, true);
  Object.setPrototypeOf(String.prototype, Object.prototype);
  assertEq(getFuseState().OptimizeStringPrototypeSymbolsFuse.intact, true);
  Object.setPrototypeOf(String.prototype, {});
  assertEq(getFuseState().OptimizeStringPrototypeSymbolsFuse.intact, false);
`);
