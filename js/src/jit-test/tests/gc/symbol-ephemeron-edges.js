// |jit-test| --enable-symbols-as-weakmap-keys; skip-if: getBuildConfiguration("release_or_beta")
function make(name) {
  return Symbol(name);
}
var sym1 = make('sym1');
var sym2 = make('sym2');
var dummy = make('dummy');
var wm1 = new WeakMap([[sym1, sym2]]);
var wm2 = new WeakMap([[sym2, dummy]]);
gc();
var mark_order = [wm1, sym1, wm2];
sym1 = sym2 = dummy = wm1 = wm2 = null;
gc();
