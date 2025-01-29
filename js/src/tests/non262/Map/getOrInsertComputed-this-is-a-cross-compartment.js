// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed)

const g = newGlobal({ newCompartment: true });

var map = g.eval("new Map()");

Map.prototype.getOrInsertComputed.call(map, 1, () => 2);
assertEq(map.get(1), 2);

map.getOrInsertComputed(2, () => 3);
assertEq(map.get(2), 3);

reportCompare(0, 0);