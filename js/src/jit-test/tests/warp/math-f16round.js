function fromConstant() {
  // ToFloat16(ToDouble(constant)) is folded to ToFloat16(constant).

  for (let i = 0; i < 100; ++i) {
    assertEq(Math.f16round(0), 0);
    assertEq(Math.f16round(0.1), 0.0999755859375);
    assertEq(Math.f16round(0.5), 0.5);
    assertEq(Math.f16round(1), 1);
    assertEq(Math.f16round(2049), 2048);
    assertEq(Math.f16round(65520), Infinity);

    assertEq(Math.f16round(-0), -0);
    assertEq(Math.f16round(-0.1), -0.0999755859375);
    assertEq(Math.f16round(-0.5), -0.5);
    assertEq(Math.f16round(-1), -1);
    assertEq(Math.f16round(-2049), -2048);
    assertEq(Math.f16round(-65520), -Infinity);

    assertEq(Math.f16round(NaN), NaN);
    assertEq(Math.f16round(Infinity), Infinity);
    assertEq(Math.f16round(-Infinity), -Infinity);

    assertEq(Math.f16round(0 / 0), NaN);
    assertEq(Math.f16round(1 / 0), Infinity);
    assertEq(Math.f16round(-1 / 0), -Infinity);
  }
}
for (let i = 0; i < 2; ++i) fromConstant();

function fromInt32() {
  // ToFloat16(ToDouble(int32)) is folded to ToFloat16(int32).

  // Int32 which are exactly representable as Float16.
  for (let i = 0; i <= 2048; ++i) {
    let i32 = i | 0;
    assertEq(Math.f16round(i32), i32);
  }

  // Int32 larger than 2048 are inexact.
  assertEq(Math.f16round(2049), 2048);

  // Int32 larger than 65519 are not representable.
  assertEq(Math.f16round(65519), 65504);

  // Int32 which are too large for Float16.
  for (let i = 0; i <= 100; ++i) {
    let i32 = (i + 65520) | 0;
    assertEq(Math.f16round(i32), Infinity);
  }
}
for (let i = 0; i < 2; ++i) fromInt32();

function fromFloat32() {
  // ToFloat16(ToDouble(float32)) is folded to ToFloat16(float32).

  // Float32 which are exactly representable as Float16.
  for (let i = 0; i < 1024; ++i) {
    let f32 = Math.fround(i + 0.5);
    assertEq(Math.f16round(f32), f32);
  }

  // Float32 larger than 1023.5 are inexact.
  assertEq(Math.f16round(1024.5), 1024);

  // Float32 larger than 65519 are not representable.
  assertEq(Math.f16round(65519.5), 65504);

  // Float32 which are too large for Float16.
  for (let i = 0; i <= 100; ++i) {
    let f32 = Math.fround(i + 65520.5);
    assertEq(Math.f16round(f32), Infinity);
  }
}
for (let i = 0; i < 2; ++i) fromFloat32();

function fromLoadFloat16() {
  // ToFloat16(LoadFloat16(x)) is folded to LoadFloat16(x).

  let f16 = new Float16Array([
    -Math.PI,
    -65519,
    -65520,
    -2048,
    -2049,
    -0.5,
    -0.1,
    -0,
    0,
    0.1,
    0.5,
    Math.PI,
    2048,
    2049,
    65519,
    65520,
    Infinity,
    NaN,
  ]);

  let dv = new DataView(f16.buffer);

  const nativeIsLittleEndian = new Uint8Array(new Uint16Array([1]).buffer)[0] === 1;

  for (let i = 0; i < 200; ++i) {
    let idx = i % f16.length;
    let x = f16[idx];
    let y = dv.getFloat16(idx * Float16Array.BYTES_PER_ELEMENT, nativeIsLittleEndian);

    assertEq(x, y);
    assertEq(Math.f16round(x), x);
    assertEq(Math.f16round(y), y);
  }
}
for (let i = 0; i < 2; ++i) fromLoadFloat16();
