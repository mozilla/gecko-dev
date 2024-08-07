const g = newGlobal({ newCompartment: true });

g.eval(`
var c1 = class C1 {
};

var x1 = {
  __proto__: {
    constructor: c1,
  },
};
var getterCalled = false;
var x2 = {
  __proto__: {
    get constructor() {
      getterCalled = true;
      return c1;
    },
  },
};
`);

const dbg = new Debugger();
const gDO = dbg.addDebuggee(g);

const x1DO = gDO.makeDebuggeeValue(g.x1);
const x2DO = gDO.makeDebuggeeValue(g.x2);

// An object where __proto__.constructor matches the query should match.
const c1DO = gDO.makeDebuggeeValue(g.c1);
assertEq(dbg.findObjects({ class: c1DO }).includes(x1DO), true);

// An object where __proto__.constructor is an accessor shouldn't match.
assertEq(dbg.findObjects({ class: c1DO }).includes(x2DO), false);

// The constructor getter shouldn't be called.
assertEq(g.getterCalled, false);
