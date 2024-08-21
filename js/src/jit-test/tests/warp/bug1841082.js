class C {
  constructor(n) {
    b(n);
    super[1];
  }
}
function b(n) {
  if (n > 0) {
    new C(n-1);
  }
}
for (var i = 0; i < 1000; i++) {
  b(10);
}
