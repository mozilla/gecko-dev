const g = newGlobal({ newCompartment: true });

g.eval(`
var x1 = {};
var x2 = { __proto__: null };
`);

const dbg = new Debugger();
const gDO = dbg.addDebuggee(g);

const x1DO = gDO.makeDebuggeeValue(g.x1);
const x2DO = gDO.makeDebuggeeValue(g.x2);

// A plain object with default prototype should match `Object` query.
const ObjectDO = gDO.makeDebuggeeValue(g.Object);
assertEq(dbg.findObjects({ class: ObjectDO }).includes(x1DO), true);

// An object with null prototype shouldn't match `Object` query.
assertEq(dbg.findObjects({ class: ObjectDO }).includes(x2DO), false);
