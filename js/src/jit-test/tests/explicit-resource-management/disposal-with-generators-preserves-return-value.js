// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const values = [];
  function* gen() {
    yield 1;
    using a = {
      [Symbol.dispose]: () => values.push("a")
    };
    yield 2;
    yield 3;
  }
  function testGeneratorPreservesReturnValue() {
    let it = gen();
    values.push(it.next().value);
    values.push(it.next().value);
    values.push(it.return(40).value);
  }
  testGeneratorPreservesReturnValue();
  assertArrayEq(values, [1, 2, "a", 40]);
}
