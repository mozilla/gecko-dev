const g = newGlobal({ newCompartment: true });

g.eval(`
var x1 = new Proxy({}, {});
`);

const dbg = new Debugger();
const gDO = dbg.addDebuggee(g);

const x1DO = gDO.makeDebuggeeValue(g.x1);

// Proxy instance doesn't match because of dynamic prototype.
const ProxyDO = gDO.makeDebuggeeValue(g.Proxy);
assertEq(dbg.findObjects({ class: ProxyDO }).includes(x1DO), false);
const ObjectDO = gDO.makeDebuggeeValue(g.Object);
assertEq(dbg.findObjects({ class: ObjectDO }).includes(x1DO), false);
