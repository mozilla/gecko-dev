// |jit-test| --enable-symbols-as-weakmap-keys; skip-if: getBuildConfiguration("release_or_beta")

// Test weak maps with symbols keys where the symbols are referenced in
// different zones. Currently we require all participating zones plus the
// atoms zone to be collected to collect such weakmap entries.

gczeal(0);
gczeal('CheckHeapAfterGC');

function makeSymbol(name) {
  return Symbol(name);
}

function mapSize(wm) {
  return nondeterministicGetWeakMapSize(wm);
}

// Test weak map entry with a symbol key is removed when the symbol dies.
printErr("Test 1");
let wm = new WeakMap;
assertEq(mapSize(wm), 0);
let k = makeSymbol('test1');
wm.set(k, {});
assertEq(mapSize(wm), 1);
gc();
assertEq(mapSize(wm), 1);
k = undefined;
gc();
assertEq(mapSize(wm), 0);
wm = undefined;
gc();

// Repeat the test with the key held alive from a different zone.
printErr("Test 2");
let keyZone = newGlobal({newCompartment: true});
keyZone.key = makeSymbol('test2');
let mapZone = newGlobal({newCompartment: true});
mapZone.map = undefined;
mapZone.eval(`map = new WeakMap;`);
mapZone.map.set(keyZone.key, {});
assertEq(mapSize(mapZone.map), 1);
gc();
assertEq(mapSize(mapZone.map), 1);
keyZone.key = undefined;
gc();
assertEq(mapSize(mapZone.map), 0);
keyZone = undefined;
mapZone = undefined;
gc();

// Repeat the previous test with per zone GC.
printErr("Test 3");
keyZone = newGlobal({newCompartment: true});
mapZone = newGlobal({newCompartment: true});
keyZone.eval('var key = Symbol("test2");');
mapZone.eval(`var map = new WeakMap;`);
mapZone.keyZone = keyZone;
mapZone.eval('map.set(keyZone.key, {});');
mapZone.keyZone = undefined;
gc();
assertEq(mapSize(mapZone.map), 1);

printErr("  3.1 Setup");
keyZone.key = undefined;

printErr("  3.2 Collect only main zone");
gc(this);
assertEq(mapSize(mapZone.map), 1);

printErr("  3.3 Collect only map zone");
gc(mapZone);
assertEq(mapSize(mapZone.map), 1);

printErr("  3.4 Collect only key zone");
gc(keyZone);
// Bug 1410123: Perhaps we could stop marking the key atom in the key zone at
// this point if we refine the atom bitmaps without requiring the atoms zone
// to be collected.
assertEq(mapSize(mapZone.map), 1);

printErr("  3.5 Collect map zone and atoms zone (entry not removed)");
schedulezone(mapZone);
schedulezone('atoms');
gc('zone');
assertEq(mapSize(mapZone.map), 1);

printErr("  3.6 Collect key zone and atoms zone (key no longer referenced)");
schedulezone(keyZone);
schedulezone('atoms');
gc('zone');
assertEq(mapSize(mapZone.map), 1);

printErr("  3.7 Collect only atoms zone (nothing collected)");
schedulezone('atoms');
gc('zone');
assertEq(mapSize(mapZone.map), 1);

printErr("  3.8 Collect only map zone (nothing collected)");
gc(mapZone);
assertEq(mapSize(mapZone.map), 1);

printErr("  3.9 Collect map zone and atoms zone (entry collected)");
schedulezone(mapZone);
schedulezone('atoms');
gc('zone');
assertEq(mapSize(mapZone.map), 0);

printErr("  3.10 Collect all zones (nothing more collected)");
gc();
assertEq(mapSize(mapZone.map), 0);
