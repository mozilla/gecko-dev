const g1 = newGlobal({ newCompartment: true });
const g2 = newGlobal({ newCompartment: true });

g1.eval(`
var liveCtor = function() {
};
var liveProtoWithLiveCtor = {
  constructor: liveCtor,
};
`);

g2.liveCtor = g1.liveCtor;

g2.eval(`
var deadCtor = function () {
};
var deadProtoWithLiveCtor = {
  constructor: liveCtor,
};
var deadProtoWithDeadCtor = {
  constructor: liveCtor,
};
`);

g1.deadCtor = g2.deadCtor;
g1.deadProtoWithLiveCtor = g2.deadProtoWithLiveCtor;
g1.deadProtoWithDeadCtor = g2.deadProtoWithDeadCtor;

g1.eval(`
var liveProtoWithDeadCtor = {
  constructor: deadCtor,
};
`);

g1.eval(`
var x1 = {
  __proto__: liveProtoWithLiveCtor,
};
var x2 = {
  __proto__: liveProtoWithDeadCtor,
};
var x3 = {
  __proto__: deadProtoWithLiveCtor,
};
var x4 = {
  __proto__: deadProtoWithDeadCtor,
};

nukeCCW(deadProtoWithDeadCtor);
nukeCCW(deadProtoWithLiveCtor);
nukeCCW(deadCtor);
`);

const dbg = new Debugger();
const gDO = dbg.addDebuggee(g1);

const x1DO = gDO.makeDebuggeeValue(g1.x1);
const x2DO = gDO.makeDebuggeeValue(g1.x2);
const x3DO = gDO.makeDebuggeeValue(g1.x3);
const x4DO = gDO.makeDebuggeeValue(g1.x4);

const liveCtorDO = gDO.makeDebuggeeValue(g1.liveCtor);

// If both prototype and constructor is live, it should match.
assertEq(dbg.findObjects({ class: liveCtorDO }).includes(x1DO), true);

// If the prototype is dead wrapper, it shouldn't match.
assertEq(dbg.findObjects({ class: liveCtorDO }).includes(x3DO), false);

// If the query is a dead wrapper, it should throw.
let caught = false;
try {
  dbg.findObjects({ class: g1.deadCtor });
} catch (e) {
  assertEq(e.message.includes("dead object"), true);
  caught = true;
}
assertEq(caught, true);

// If the query is a debuggee value with dead wrapper, it should throw.
const reallyDeadCtorDO = gDO.makeDebuggeeValue(g1.deadCtor);
caught = false;
try {
  dbg.findObjects({ class: reallyDeadCtorDO });
} catch (e) {
  assertEq(e.message.includes("dead object"), true);
  caught = true;
}
assertEq(caught, true);

// If the constructor is dead wrapper, it shouldn't match.
const deadCtorDO = gDO.makeDebuggeeValue(g2.deadCtor);
assertEq(dbg.findObjects({ class: deadCtorDO }).includes(x2DO), false);
assertEq(dbg.findObjects({ class: deadCtorDO }).includes(x4DO), false);

// If the prototype is dead wrapper, it shouldn't match.
const deadProtoWithLiveCtorDO = gDO.makeDebuggeeValue(g2.deadProtoWithLiveCtor);
assertEq(dbg.findObjects({ class: deadProtoWithLiveCtorDO }).includes(x3DO), false);
const deadProtoWithDeadCtorDO = gDO.makeDebuggeeValue(g2.deadProtoWithDeadCtor);
assertEq(dbg.findObjects({ class: deadProtoWithLiveCtorDO }).includes(x4DO), false);
