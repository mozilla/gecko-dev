var code = cacheEntry(`
function f() {
  class A {
    constructor() {}
  }
  new A();
  relazifyFunctions();
  new A();
  relazifyFunctions();
}
f();
`);

evaluate(code, {saveBytecodeWithDelazifications: { value: true } });
evaluate(code, {loadBytecode: { value: true } });
