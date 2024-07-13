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
