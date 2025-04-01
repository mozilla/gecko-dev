// Mutating certain properties pops the fuse. Changing the enumerable attribute is fine.
function testProp(propName) {
  newGlobal().evaluate(`
    assertEq(getFuseState().OptimizeRegExpPrototypeFuse.intact, true);
    Object.defineProperty(RegExp.prototype, ${propName}, {enumerable: true});
    assertEq(getFuseState().OptimizeRegExpPrototypeFuse.intact, true);
    Object.defineProperty(RegExp.prototype, ${propName}, {value:null});
    assertEq(getFuseState().OptimizeRegExpPrototypeFuse.intact, false);
  `);
}

// Getters.
testProp(`"flags"`);
testProp(`"global"`);
testProp(`"hasIndices"`);
testProp(`"ignoreCase"`);
testProp(`"multiline"`);
testProp(`"sticky"`);
testProp(`"unicode"`);
testProp(`"unicodeSets"`);
testProp(`"dotAll"`);

// Data properties.
testProp(`"exec"`);
testProp(`Symbol.match`);
testProp(`Symbol.search`);
