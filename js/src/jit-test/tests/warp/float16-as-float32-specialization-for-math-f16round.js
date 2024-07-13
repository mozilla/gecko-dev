function float32(i) {
  // Unbox to Int32.
  i = i|0;

  // Float32 addition.
  let x = Math.fround(i + 0.1);

  // Float32 square root.
  let y = Math.fround(Math.sqrt(x));

  // Return as Float16.
  return Math.f16round(y);
}

function float64(i) {
  // Unbox to Int32.
  i = i|0;

  // Float32 addition.
  let x = Math.fround(i + 0.1);

  // Float64 square root.
  let y = Math.sqrt(x);

  // Return as Float16.
  return Math.f16round(y);
}

function toBaseline(f) {
  let source = f.toString();
  assertEq(source.at(-1), "}");

  // Add with-statement to disable Ion compilation.
  source = source.slice(0, -1) + "; with ({}); }";

  return Function(`return ${source};`)();
}

// Different results are expected for these inputs:
//
// Input    Float64-SQRT  Float32-SQRT
// -----------------------------------
// 1527     39.09375      39.0625
// 16464    128.375       128.25
// 18581    136.375       136.25
// 20826    144.375       144.25
// 23199    152.375       152.25
// 25700    160.375       160.25
// 28329    168.375       168.25
// 31086    176.375       176.25
//
// Limit execution to 1550 to avoid spending too much time on this single test.
//
// 1550 iterations should still be enough to allow tiering up to Ion, at least
// under eager compilation settings.
const LIMIT = 1550;

let float32_baseline = toBaseline(float32);
let float64_baseline = toBaseline(float64);

let n = 0;
for (let i = 0; i < LIMIT; ++i) {
  let x = float32(i);
  assertEq(x, float32_baseline(i));

  let y = float64(i);
  assertEq(y, float64_baseline(i));

  if (x !== y) {
    n++;
  }
}
assertEq(n, 1);
