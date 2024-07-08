let g = newGlobal({sameZoneAs: this});
let dbg = g.Debugger(this);
dbg.memory.trackingAllocationSites = true;

function bar() {
  bar()
}
try {
  bar();
} catch {}
