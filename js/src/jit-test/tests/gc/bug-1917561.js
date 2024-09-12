gczeal(0);
gczeal(10, 3);
gczeal(11, 2);
test(`
  var lfOffThreadGlobal = newGlobal();
  for (lfLocal in this) {}
  var lfOffThreadGlobal = newGlobal();
  for (lfLocal in this) {}
  enqueueMark('set-color-gray');
  enqueueMark(newGlobal());
  assertEquals();
`);
function test(src) {
    try {
        evaluate(src);
    } catch (lfVare) {
        new Set([]);
    }
}
