// |jit-test| --fast-warmup; --ion-inlining=off; --no-threads

var classes = [];
for (var i = 0; i < 16; i++) {
  class C extends Uint8Array {
    constructor(n) { super(n); }
    0 = 1;
  }
  classes.push(C);
}

function foo(classIdx, size) {
  return new classes[classIdx](size);
}

// Compile
for (var i = 0; i < 100; i++) {
  foo(i % 7, 5);
}

for (var i = 0; i < 10; i++) {
  try {
    foo(7, 0);
  } catch {}
}

for (var i = 0; i < 20; i++) {
  foo(i % 16, 5);
}
