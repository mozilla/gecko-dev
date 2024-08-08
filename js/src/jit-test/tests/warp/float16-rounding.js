const tests = [
  // Float16 subnormal numbers.
  {
    value: 2.9802322387695312e-8,
    f64: 0x3e60_0000_0000_0000n,
    f32: 0x3300_0000,
    f16: 0x0,
  },
  {
    value: 2.980232238769532e-8,
    f64: 0x3e60_0000_0000_0001n,
    f32: 0x3300_0000,
    f16: 0x1,
  },

  {
    value: 8.940696716308592e-8,
    f64: 0x3e77_ffff_ffff_ffffn,
    f32: 0x33c0_0000,
    f16: 0x1,
  },
  {
    value: 8.940696716308594e-8,
    f64: 0x3e78_0000_0000_0000n,
    f32: 0x33c0_0000,
    f16: 0x2,
  },

  {
    value: 0.000060945749282836914,
    f64: 0x3f0f_f400_0000_0000n,
    f32: 0x387f_a000,
    f16: 0x3fe,
  },
  {
    value: 0.00006094574928283692,
    f64: 0x3f0f_f400_0000_0001n,
    f32: 0x387f_a000,
    f16: 0x3ff,
  },

  {
    value: 0.0000610053539276123,
    f64: 0x3f0f_fbff_ffff_ffffn,
    f32: 0x387f_e000,
    f16: 0x3ff,
  },
  {
    value: 0.000061005353927612305,
    f64: 0x3f0f_fc00_0000_0000n,
    f32: 0x387f_e000,
    f16: 0x400,
  },

  // Float16 normal numbers.
  {
    value: 0.000061035154431010596,
    f64: 0x3f0f_ffff_f000_0000n,
    f32: 0x3880_0000,
    f16: 0x400,
  },
  {
    value: 0.00006103515625,
    f64: 0x3f10_0000_0000_0000n,
    f32: 0x3880_0000,
    f16: 0x400,
  },
  {
    value: 0.0000610649585723877,
    f64: 0x3f10_0200_0000_0000n,
    f32: 0x3880_1000,
    f16: 0x400,
  },
  {
    value: 0.00006106495857238771,
    f64: 0x3f10_0200_0000_0001n,
    f32: 0x3880_1000,
    f16: 0x401,
  },
  {
    value: 0.00006112456321716307,
    f64: 0x3f10_05ff_ffff_ffffn,
    f32: 0x3880_3000,
    f16: 0x401,
  },

  // Underflow to zero.
  {
    value: 2.980232594040899e-8,
    f64: 0x3e60_0000_2000_0000n,
    f32: 0x3300_0001,
    f16: 0x1,
  },
  {
    value: 2.9802322387695312e-8,
    f64: 0x3e60_0000_0000_0000n,
    f32: 0x3300_0000,
    f16: 0x0,
  },
  {
    value: 2.9802320611338473e-8,
    f64: 0x3e5f_ffff_e000_0000n,
    f32: 0x32ff_ffff,
    f16: 0x0,
  },

  // Overflow to infinity.
  {
    value: 65536,
    f64: 0x40f0_0000_0000_0000n,
    f32: 0x4780_0000,
    f16: 0x7c00,
  },
  {
    value: 65520,
    f64: 0x40ef_fe00_0000_0000n,
    f32: 0x477f_f000,
    f16: 0x7c00,
  },
  {
    value: 65504,
    f64: 0x40ef_fc00_0000_0000n,
    f32: 0x477f_e000,
    f16: 0x7bff,
  },
];

const ta_f64 = new Float64Array(1);
const ta_f32 = new Float32Array(1);
const ta_f16 = new Float16Array(1);

const ta_u64 = new BigUint64Array(ta_f64.buffer);
const ta_u32 = new Uint32Array(ta_f32.buffer);
const ta_u16 = new Uint16Array(ta_f16.buffer);

for (let i = 0; i < 1000; ++i) {
  let {value, f64, f32, f16} = tests[i % tests.length];

  ta_f64[0] = value;
  assertEq(ta_u64[0], f64);

  ta_f32[0] = value;
  assertEq(ta_u32[0], f32);

  ta_f16[0] = value;
  assertEq(ta_u16[0], f16);

  assertEq(Math.f16round(value), ta_f16[0]);
}

// Test negative case.
for (let i = 0; i < 1000; ++i) {
  let {value, f64, f32, f16} = tests[i % tests.length];

  value = value * -1;
  f64 = (f64 | 0x8000_0000_0000_0000n);
  f32 = (f32 | 0x8000_0000) >>> 0;
  f16 = (f16 | 0x8000);

  ta_f64[0] = value;
  assertEq(ta_u64[0], f64);

  ta_f32[0] = value;
  assertEq(ta_u32[0], f32);

  ta_f16[0] = value;
  assertEq(ta_u16[0], f16);

  assertEq(Math.f16round(value), ta_f16[0]);
}
