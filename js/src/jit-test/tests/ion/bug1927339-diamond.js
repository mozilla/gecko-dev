// |jit-test| --fast-warmup; --gc-zeal=10; --cache-ir-stubs=off; --no-threads
function trigger(i) {
  if (i === 49) {
    gc();
  }
}
function f() {
  for (let i = 0; i < 50; i++) {
    const b0 = i === 1000;
    const b1 = i === 2000;
    const b2 = !!b1;
    trigger(i);
    if (b2 ? b0 : b1) {
      break;
    }
  }
}
f();
