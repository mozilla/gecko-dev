// |jit-test| allow-oom

{
  const a = 1 << 28;
  const d = a / 8;
  let f = [];
  for (let g = 0; g < d; g++) {
    f[g] = g;
  }
  const g = a - 6;
  f[g] = g;
}
