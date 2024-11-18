function foo() {
  new Float64Array(0x80000001)()
}

for (var i = 0; i < 100; i++) {
  try {
    foo();
  } catch {}
}
