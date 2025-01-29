// |reftest| shell-option(--enable-upsert) skip-if(!WeakMap.prototype.getOrInsertComputed)

const g = newGlobal({ newCompartment: true });

var map = g.eval("new WeakMap()");
var foo = {};
var bar = {};

WeakMap.prototype.getOrInsertComputed.call(map, foo, () => 2);
assertEq(map.get(foo), 2);

map.getOrInsertComputed(bar, () => 3);
assertEq(map.get(bar), 3);

reportCompare(0, 0);