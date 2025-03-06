// Valid radices for the BigInt constructor.
const radices = [
  {
    radix: 2,
    prefix: "0b",
  },
  {
    radix: 8,
    prefix: "0o",
  },
  {
    radix: 10,
    prefix: "",
  },
  {
    radix: 16,
    prefix: "0x",
  },
];

// Leading zeroes are ignored.
const zeroes = [
  "", "0", "00",
];

// Generate BigInt literals with up to 200 digits to cover the case when the
// result has more than one BigInt::Digit.
function* literals(radix, prefix) {
  const digits = "0123456789abcdefghijklmnopqrstuvwxyz";
  assertEq(radix < digits.length, true);

  let n = 0n;
  let s = prefix;
  for (let i = 1; i <= 200; ++i) {
    let d = i % radix;
    n = n * BigInt(radix) + BigInt(d);
    s += digits[d];

    yield [s, n];
  }
}

for (let {radix, prefix} of radices) {
  for (let zero of zeroes) {
    for (let [s, n] of literals(radix, prefix + zero)) {
      assertEq(BigInt(s), n, `literal: "${s}"`);
    }
  }
}
