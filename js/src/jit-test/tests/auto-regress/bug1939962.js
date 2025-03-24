function test() {
  var fn = Function("x,".repeat(5000), "");
  for (var i = 0; i < 100; i++) {
    fn();
  }
}
test();
