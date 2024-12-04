code = cacheEntry(`
  function f() {
    (function () {})
  };
  f()
  `);
evaluate(code, { saveBytecodeWithDelazifications: { value: true } });
