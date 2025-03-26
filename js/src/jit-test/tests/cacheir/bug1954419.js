function foo(...rest) {
  rest[3] = 0;
}

with ({}) {}
for (var i = 0; i < 500; i++) {
  foo();
}
