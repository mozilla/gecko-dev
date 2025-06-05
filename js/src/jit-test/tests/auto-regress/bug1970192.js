// |jit-test| --fast-warmup

var f = Iterator.prototype.drop;

function g() {
  f();
}

for (var k = 0; k < 100; ++k) {
  try {
    g();
  } catch {}
}
