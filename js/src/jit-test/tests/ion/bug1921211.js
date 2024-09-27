// |jit-test| --fast-warmup; --ion-pruning=off; --no-threads
function test() {
  var arr = new BigInt64Array(117);
  for (var i = 0; i < 4; i++) {
    for (var j = 0; j < 4; j++) {
      var bigInt1 = arr[0];
      var a1 = Object.is(arr[i], bigInt1);
      var a2 = [...arr];
      try { throw 1; } catch (e) { }
    }
  }
}
test();
