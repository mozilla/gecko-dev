const g = newGlobal({ newCompartment: true });

g.eval(`
var c1 = class C1 {
};
var c2 = class C2 extends c1 {
};

var x1 = new c1();
var x2 = new c2();
`);

const dbg = new Debugger();
const gDO = dbg.addDebuggee(g);

const x1DO = gDO.makeDebuggeeValue(g.x1);
const x2DO = gDO.makeDebuggeeValue(g.x2);

// The direct ctor/proto should match.
const c1DO = gDO.makeDebuggeeValue(g.c1);
assertEq(dbg.findObjects({ class: c1DO }).includes(x1DO), true);
const c1ProtoDO = gDO.makeDebuggeeValue(g.c1.prototype);
assertEq(dbg.findObjects({ class: c1ProtoDO }).includes(x1DO), true);

const c2DO = gDO.makeDebuggeeValue(g.c2);
assertEq(dbg.findObjects({ class: c2DO }).includes(x2DO), true);
const c2ProtoDO = gDO.makeDebuggeeValue(g.c2.prototype);
assertEq(dbg.findObjects({ class: c2ProtoDO }).includes(x2DO), true);

// The super ctor/proto should match.
assertEq(dbg.findObjects({ class: c1DO }).includes(x2DO), true);
assertEq(dbg.findObjects({ class: c1ProtoDO }).includes(x2DO), true);

// Subclass's prototype is instance of superclass's prototype.
assertEq(dbg.findObjects({ class: c1DO }).includes(c2ProtoDO), true);
assertEq(dbg.findObjects({ class: c1ProtoDO }).includes(c2ProtoDO), true);

// Subclass's ctor is instance of superclass's ctor.
assertEq(dbg.findObjects({ class: c1DO }).includes(c2DO), true);

// Subclass's ctor has no relation wtih superclass's prototype.
assertEq(dbg.findObjects({ class: c1ProtoDO }).includes(c2DO), false);
