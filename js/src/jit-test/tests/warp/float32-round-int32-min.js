// |jit-test| test-also=--no-sse4

let ta = new Float32Array([
  -2147483648.0,
  -2147483648.0,
]);

for (let i = 0; i < 1000; i++) {
  assertEq(Math.round(ta[i & 1]), -2147483648.0);
}
