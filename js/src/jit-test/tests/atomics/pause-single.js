// |jit-test| --enable-atomics-pause; skip-if: !Atomics.pause

// Call Atomics.pause with no arguments.
function noArguments() {
  for (let i = 0; i < 1000; ++i) {
    Atomics.pause();
  }
}
for (let i = 0; i < 2; ++i) noArguments();

// Call Atomics.pause with the constant integer zero.
function zero() {
  for (let i = 0; i < 1000; ++i) {
    Atomics.pause(0);
  }
}
for (let i = 0; i < 2; ++i) zero();

// Call Atomics.pause with an linear increasing integer.
function increasingLinear() {
  for (let i = 0; i < 1000; ++i) {
    Atomics.pause(i);
  }
}
for (let i = 0; i < 2; ++i) increasingLinear();

// Call Atomics.pause with an linear decreasing integer.
function decreasingLinear() {
  for (let i = 0; i < 1000; ++i) {
    Atomics.pause(-i);
  }
}
for (let i = 0; i < 2; ++i) decreasingLinear();

// Call Atomics.pause with an exponentially increasing integer.
function increasingExp() {
  for (let i = 0; i < 1000; ++i) {
    Atomics.pause(2 ** Math.min(i >> 1, 10));
  }
}
for (let i = 0; i < 2; ++i) increasingExp();

// Call Atomics.pause with an exponentially decreasing integer.
function decreasingExp() {
  for (let i = 0; i < 1000; ++i) {
    Atomics.pause(-(2 ** Math.min(i >> 1, 10)));
  }
}
for (let i = 0; i < 2; ++i) decreasingExp();
