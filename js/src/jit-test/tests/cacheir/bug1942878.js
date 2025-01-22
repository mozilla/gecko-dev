let a = {}

function foo(i) {
  return a.hasOwnProperty(-i);
}

with ({}) {}
for (var i = 0; i < 2000; i++) {
  foo(1 + i % 6);
}

for (let i = 1; i < 5000; i++) {
  foo(1 + i % 7);
}
