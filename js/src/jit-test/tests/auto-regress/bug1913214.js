load(libdir + "asserts.js");

for (let i = 0; i <= 1000; ++i) {
  let v = BigInt(i ** 4);
  if (i === 1000) {
    assertEq(v, 1000000000000n);
  }
}

function test() {
  for (let i = 0; i <= 1000; ++i) {
    let v = BigInt(2 ** ((i === 1000) * 1024));
    if (i === 1000) {
      assertEq(v, 1000000000000n);
    }
  }
}
assertThrowsInstanceOf(test, RangeError);
