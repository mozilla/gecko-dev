// |jit-test| --enable-symbols-as-weakmap-keys; skip-if: getBuildConfiguration("release_or_beta")

function makeSymbol(name) {
  return Symbol(name);
}

let key = makeSymbol('test');
let mapZone = newGlobal({newCompartment: true});
mapZone.map = undefined;
mapZone.eval(`map = new WeakMap;`);
mapZone.map.set(key, {});
key = undefined;

schedulezone('atoms');
schedulezone(mapZone);
gc('zone');
