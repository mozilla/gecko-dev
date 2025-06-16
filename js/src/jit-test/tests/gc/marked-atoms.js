// Test basics of atom marking and check the isAtomMarked function works.

gczeal(0);
let global = newGlobal({newCompartment: true});
global.eval('var x = {}');
gc();

// Make an anonymous symbol and mark it in the global.
let atom = Symbol();
assertEq(isAtomMarked(this, atom), true);
assertEq(isAtomMarked(global, atom), false);
global.x[atom] = 0;
assertEq(isAtomMarked(global, atom), true);

// Make a named symbol (plus an atom for the description) and mark it.
let sym = Symbol("baz");
assertEq(isAtomMarked(this, sym), true);
assertEq(isAtomMarked(global, sym), false);
global.x[sym] = 0;
assertEq(isAtomMarked(global, sym), true);
