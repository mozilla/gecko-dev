load(libdir + 'asserts.js');

const g = newGlobal({ newCompartment: true });

g.eval(`
var arr = [];
`);

const dbg = new Debugger();

// The query should be Debugger.Object
assertThrowsInstanceOf(() => {
  dbg.findObjects({ class: g.Array });
}, TypeError);
assertThrowsInstanceOf(() => {
  dbg.findObjects({ class: g.arr });
}, TypeError);
assertThrowsInstanceOf(() => {
  dbg.findObjects({ class: null });
}, TypeError);
assertThrowsInstanceOf(() => {
  dbg.findObjects({ class: {} });
}, TypeError);
