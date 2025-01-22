const inputs = [NaN, "0"];

for (let j = 0; j < inputs.length; ++j) {
  for (let k = 0; k < 10; ++k) {
    let x = +inputs[j];
    assertEq(Math.sign(x), x);
  }
}
