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
  // Check that changing the Symbol.toPrimitive function of Date.prototype does not affect the inlined Date subtraction.
  const testCases = [[new Date("2024-09-20T19:54:27.432Z"), new Date("2024-09-20T19:54:27.427Z"), 5],
                 [new Date("2024-09-20T19:54:27.432Z"), 1726862067427, 5],
                 [1726862067427, new Date("2024-09-20T19:54:27.432Z"), -5]];
  const funDateSub = (a, b) => { return a - b; }
  warmup(funDateSub, testCases);

  const originalDateToPrimitive = Date.prototype[Symbol.toPrimitive];
  let counter = 0;
  Object.defineProperty(Date.prototype, Symbol.toPrimitive, {
    value: function(hint) {
      counter++;
      return originalDateToPrimitive.call(this, hint);
    },
    configurable: true,
  });

  assertEq(funDateSub(new Date("2024-09-20T19:54:27.432Z"), new Date("2024-09-20T19:54:27.422Z")), 10);
  assertEq(counter, 2);
}

{
  // Check that a pre-existing Date.prototype[Symbol.toPrimitive] override does not affect the inlined Date subtraction.
  let counter = 0;
  const originalDateToPrimitive = Date.prototype[Symbol.toPrimitive];
  Object.defineProperty(Date.prototype, Symbol.toPrimitive, {
    value: function(hint) {
      counter++;
      return originalDateToPrimitive.call(this, hint);
    },
  });

  const funDateSub = (a, b) => { return a - b; }
  assertEq(funDateSub(new Date("2024-09-20T19:54:27.432Z"), new Date("2024-09-20T19:54:27.422Z")), 10);
  assertEq(counter, 2);

  const testCases = [[new Date("2024-09-20T19:54:27.432Z"), new Date("2024-09-20T19:54:27.427Z"), 5],
                 [new Date("2024-09-20T19:54:27.432Z"), 1726862067427, 5],
                 [1726862067427, new Date("2024-09-20T19:54:27.432Z"), -5]];
  warmup(funDateSub, testCases);
  counter = 0;

  assertEq(funDateSub(new Date("2024-09-20T19:54:27.432Z"), new Date("2024-09-20T19:54:27.422Z")), 10);
  assertEq(funDateSub(new Date("2024-09-20T19:54:27.432Z"), 1726862067427), 5);
  assertEq(counter, 3);
}

{
  // Check that using extended date classes does not affect the inlined Date subtraction.
  const testCases = [[new Date("2024-09-20T19:54:27.432Z"), new Date("2024-09-20T19:54:27.427Z"), 5],
                 [new Date("2024-09-20T19:54:27.432Z"), 1726862067427, 5],
                 [1726862067427, new Date("2024-09-20T19:54:27.432Z"), -5]];
  const funDateSub = (a, b) => { return a - b; }
  warmup(funDateSub, testCases);

  let counter = 0;
  class MyDate extends Date {
    [Symbol.toPrimitive]() {
      counter++;
      return super.valueOf();
    }
  }

  assertEq(funDateSub(new MyDate("2024-09-20T19:54:27.432Z"), new MyDate("2024-09-20T19:54:27.422Z")), 10);
  assertEq(funDateSub(1726862067427, new MyDate("2024-09-20T19:54:27.432Z")), -5);
  assertEq(counter, 3);
}
