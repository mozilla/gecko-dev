class Cls {
  constructor(x) {
    f(x);
    super[this];
  }
}
function f(x) {
  if (x > 0) {
    new Cls(x - 1);
  }
}
function test() {
  for (var i = 0; i < 1000; i++) {
    f(5);
  }
}
test();
