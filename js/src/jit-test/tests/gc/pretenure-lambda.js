let r = [];

let h = function (x) {
  let y = x;
  r.push(() => x);
}

function i(x) {
  for (let i = 0; i < x; i++) {
    h(i);
  }
}

gc();

print("Warm up");
i(400);
minorgc();

print("Baseline");
i(400);
minorgc();

print("Run");
i(5000);  // Warmup count is 1500
