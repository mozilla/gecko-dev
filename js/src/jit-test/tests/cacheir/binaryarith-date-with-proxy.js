setJitCompilerOption('ion.forceinlineCaches', 1);

function warmup(fun, input_array) {
  for (var index = 0; index < input_array.length; index++) {
      input = input_array[index];
      input_lhs = input[0];
      input_rhs = input[1];
      output    = input[2];
      for (var i = 0; i < 30; i++) {
          var y = fun(input_lhs, input_rhs);
          assertEq(y, output)
      }
  }
}

{
  // check that binding a date object with a proxy does not affect the inlined Date subtraction.
  const testCases = [[new Date("2024-09-20T19:54:27.432Z"), new Date("2024-09-20T19:54:27.427Z"), 5],
                 [new Date("2024-09-20T19:54:27.432Z"), 1726862067427, 5],
                 [1726862067427, new Date("2024-09-20T19:54:27.432Z"), -5]];
  const funDateSub = (a, b) => { return a - b; }
  warmup(funDateSub, testCases);

  let proxyCalled = false;
  const proxy = new Proxy(new Date("2024-09-20T19:54:27.432Z"), {
    get: function(target, prop, receiver) {
      if (prop === "valueOf") {
        proxyCalled = true;
        const fn = function() {
          return 1726862067433;
        }
        return fn.bind(target);
      }
      return Reflect.get(target, prop, receiver);
    }
  });

  assertEq(funDateSub(proxy, new Date("2024-09-20T19:54:27.427Z")), 6);
  assertEq(proxyCalled, true);

  proxyCalled = false;
  assertEq(funDateSub(proxy, 1726862067427), 6);
  assertEq(proxyCalled, true);
}
