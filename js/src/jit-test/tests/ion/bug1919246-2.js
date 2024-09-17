// |jit-test| --fast-warmup
function f() {
  var i = 0;
  while (true) {
    for (let a = 2; a;) {
      for (c = 0; c < 10;) {
        if (i++ > 2500) return;
      }
    }
  }
}
f();
