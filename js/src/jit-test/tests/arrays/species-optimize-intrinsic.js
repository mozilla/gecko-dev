var canOptimize = getSelfHostedValue("CanOptimizeArraySpecies");

function test1() {
  for (var i = 0; i < 20; i++) {
    // If one of these starts failing, we should probably change how
    // CanOptimizeArraySpecies is implemented to avoid perf problems!
    assertEq(canOptimize([1, 2, 3]), true);
    var a = [];
    for (var j = 0; j < 10; j++) {
      a.push(j);
    }
    assertEq(canOptimize(a), true);
    assertEq(canOptimize(a.slice()), true);
    assertEq(canOptimize(a.map(x => x + 1)), true);

    // These aren't plain arrays.
    assertEq(canOptimize({}), false);
    Object.setPrototypeOf(a, Object.create(Array.prototype));
    assertEq(canOptimize(a), false);
    a = [];
    a.constructor = function() {};
    assertEq(canOptimize(a), false);
  }
}
test1();

function test2() {
  for (var i = 0; i < 20; i++) {
    assertEq(canOptimize([1, 2, 3]), i <= 16);
    if (i === 16) {
      // Pop the fuse.
      Array.prototype.constructor = Object;
    }
  }
}
test2();
