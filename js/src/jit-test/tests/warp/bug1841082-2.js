class C {
  constructor(n) {
    b(C,n);
    super[1];
  }
}

function b(target, n) {
  if (n > 0) {
    new target(n-1);
  }
}

// Make sure there are two ICs in the chain.
class C2 {}
for (var i = 0; i < 50; i++) {
  b(C2, 1)
}

for (var i = 0; i < 2000; i++) {
  b(C, 10);
}
