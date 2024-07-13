const nativeIsLittleEndian = new Uint8Array(new Uint16Array([1]).buffer)[0] === 1;

function float32(dv, i) {
  // Unbox to Int32.
  i = i|0;

  // Float32 addition.
  let x = Math.fround(i + 0.1);

  // Float32 square root.
  let y = Math.fround(Math.sqrt(x));

  // Store as Float16.
  dv.setFloat16(0, y, nativeIsLittleEndian);
}

function float64(dv, i) {
  // Unbox to Int32.
  i = i|0;

  // Float32 addition.
  let x = Math.fround(i + 0.1);

  // Float64 square root.
  let y = Math.sqrt(x);

  // Store as Float16.
  dv.setFloat16(0, y, nativeIsLittleEndian);
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

let dv = new DataView(new ArrayBuffer(Float16Array.BYTES_PER_ELEMENT));

let n = 0;
for (let i = 0; i < LIMIT; ++i) {
  float32(dv, i);
  let x = dv.getUint16(0, nativeIsLittleEndian);

  float32_baseline(dv, i);
  assertEq(x, dv.getUint16(0, nativeIsLittleEndian));

  float64(dv, i);
  let y = dv.getUint16(0, nativeIsLittleEndian);

  float64_baseline(dv, i);
  assertEq(y, dv.getUint16(0, nativeIsLittleEndian));

  if (x !== y) {
    n++;
  }
}
assertEq(n, 1);
