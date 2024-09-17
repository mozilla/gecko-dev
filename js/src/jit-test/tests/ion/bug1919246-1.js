// |jit-test| --fast-warmup
function test() {
  var c = [1];
  for (var d in c) {
    for (var i = 0; i < 30; i++) {
      try {
        a.push(g);
      } catch (e) { }
      for (var j = 0; j < 30; j++) { }
    }
  }
}
test();
