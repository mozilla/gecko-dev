function f() {
  for (var i = 0; i < 5; i++) {
    var x = new Map;
    gc();
    x.set({}, {});
  }
}
oomTest(f);
