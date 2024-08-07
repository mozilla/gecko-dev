const g = newGlobal({ newCompartment: true });

g.eval(`
var arr = [];
`);

const dbg = new Debugger();
const gDO = dbg.addDebuggee(g);

const arrDO = gDO.makeDebuggeeValue(g.arr);

// The direct ctor/proto should match.
const ArrayDO = gDO.makeDebuggeeValue(g.Array);
assertEq(dbg.findObjects({ class: ArrayDO }).includes(arrDO), true);
const ArrayProtoDO = gDO.makeDebuggeeValue(g.Array.prototype);
assertEq(dbg.findObjects({ class: ArrayProtoDO }).includes(arrDO), true);

// The ctor/proto in the prototype chain should match.
const ObjectDO = gDO.makeDebuggeeValue(g.Object);
assertEq(dbg.findObjects({ class: ObjectDO }).includes(arrDO), true);
const ObjectProtoDO = gDO.makeDebuggeeValue(g.Object.prototype);
assertEq(dbg.findObjects({ class: ObjectProtoDO }).includes(arrDO), true);

// Unrelated ctor/proto shouldn't match.
const RegExpDO = gDO.makeDebuggeeValue(g.RegExp);
assertEq(dbg.findObjects({ class: RegExpDO }).includes(arrDO), false);
const RegExpProtoDO = gDO.makeDebuggeeValue(g.RegExp.prototype);
assertEq(dbg.findObjects({ class: RegExpProtoDO }).includes(arrDO), false);

// The object itself shouldn't match.
assertEq(dbg.findObjects({ class: arrDO }).includes(arrDO), false);

// The ctor/proto from different global shouldn't match.
const OtherArrayDO = gDO.makeDebuggeeValue(Array);
assertEq(dbg.findObjects({ class: OtherArrayDO }).includes(arrDO), false);
const OtherArrayProtoDO = gDO.makeDebuggeeValue(Array.prototype);
assertEq(dbg.findObjects({ class: OtherArrayProtoDO }).includes(arrDO), false);
