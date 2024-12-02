var g = newGlobal({newCompartment: true});
var dbg = g.Debugger(this);
var c = 0;
oomTest(() => {
  if (c++ > 70) return;
  dbg.findObjects();
});
