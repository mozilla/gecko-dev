// Test weak maps with keys in different zones, checking whether entries are
// collected when we expect. The existance of CCWs from uncollected zones
// keeps keys alive and prevents entries from being collected.

function mapSize(wm) {
  // Note: Using nondeterministicGetWeakMapKeys will create CCWs to the keys
  // if they are in a different compartment.
  return nondeterministicGetWeakMapSize(wm);
}

let keyZone = newGlobal({newCompartment: true});
let mapZone = newGlobal({newCompartment: true});

printErr("Test 1: Full GC");

keyZone.eval('var key = makeFinalizeObserver();');
mapZone.eval('var map = new WeakMap;');
mapZone.map.set(keyZone.key, {});
let initialCount = finalizeCount();
assertEq(mapSize(mapZone.map), 1);
gc();
assertEq(finalizeCount(), initialCount);
assertEq(mapSize(mapZone.map), 1);
keyZone.key = undefined;
gc();
assertEq(finalizeCount(), initialCount + 1);
assertEq(mapSize(mapZone.map), 0);
mapZone.map = undefined;
gc();

printErr("Test 2: Zone GC");
keyZone.eval('var key = makeFinalizeObserver();');
mapZone.eval('var map = new WeakMap;');
mapZone.keyZone = keyZone;
mapZone.eval('map.set(keyZone.key, {});');
mapZone.keyZone = undefined;
assertEq(mapSize(mapZone.map), 1);
initialCount = finalizeCount();

printErr("  2.1 Setup");
gc();
keyZone.key = undefined;
assertEq(finalizeCount(), initialCount);
assertEq(mapSize(mapZone.map), 1);

printErr("  2.2 Collect only main zone (key not collected)");
gc(this);
assertEq(finalizeCount(), initialCount);
assertEq(mapSize(mapZone.map), 1);

printErr("  2.3 Collect only map zone (key not collected)");
gc(mapZone);
assertEq(finalizeCount(), initialCount);
assertEq(mapSize(mapZone.map), 1);

printErr("  2.4 Collect only key zone (key not collected)");
gc(keyZone);
assertEq(finalizeCount(), initialCount);
assertEq(mapSize(mapZone.map), 1);

printErr("  2.5 Collect key zone and map zone (key collected)");
schedulezone(keyZone);
schedulezone(mapZone);
gc('zone');
assertEq(finalizeCount(), initialCount + 1);
assertEq(mapSize(mapZone.map), 0);
