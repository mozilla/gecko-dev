// |jit-test| --fuzzing-safe; --ion-offthread-compile=off
gczeal(0);

let g = newGlobal({newCompartment: true});
let dbg = new Debugger(g);

dbg.collectCoverageInfo = true;
g.eval("0");

// Start a GC in the debugger's zone and yield after sweeping objects.
schedulezone(g);
gczeal(22);
startgc(100);

dbg.collectCoverageInfo = false;
