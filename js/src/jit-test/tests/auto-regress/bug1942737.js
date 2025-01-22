const ta = new Int32Array(4);
for (let i = 0; i < 100; ++i) {
  let index = i & 3;
  let f32Index = Math.fround(index);
  assertEq(ta[f32Index], ta[index]);
}
