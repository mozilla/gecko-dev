var shapes = [];
for (var i = 0; i < 8; i++) {
  shapes.push({x: 1, ["y" + i]: 1});
}

function foo(o, o2, cond) {
  var result = 0;
  for (var i = 0; i < 2; i++) {
    if (cond) {
      result += o.x; // hoistable guard
    }
  }
  result += o2.x; // folded stub
  return result;
}

// Ion-compile
with ({}) {}
for (var i = 0; i < 2000; i++) {
  foo({x: 1}, shapes[i%6], i % 2 == 0);
}

// Bail out in LICM-hoisted guard.
// Hit unrelated fallback and add shape to folded stub
foo(undefined, shapes[6], false);

// Get numFixableBailouts to 9
for (var i = 0; i < 9; i++) {
  foo(undefined, shapes[6], false);
}

// Bail out in non-LICM guard. Invalidate.
// Add shape to folded stub.
foo({x:1}, shapes[7], false);

// Recompile.
for (var i = 0; i < 2000; i++) {
  foo({x: 1}, shapes[i%8], i % 2 == 0);
}
