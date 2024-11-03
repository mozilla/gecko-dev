// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

class CustomError extends Error {}

{
  const values = [];
  let caught = false;
  function* gen() {
    yield 1;
    using a = {
      [Symbol.dispose]() {
        values.push("a");
        throw new CustomError();
      }
    }
    yield 2;
    yield 3;
  }
  function testGeneratorDoesntExposeMagicValue() {
    let it = gen();
    values.push(it.next().value);
    values.push(it.next().value);
    try {
      values.push(it.return(40).value);
    } catch (e) {
      caught = true;
      // The "generator closing" is represented as a throw completion with a magic value.
      // The magic value shouldn't be captured as a suppressed error and exposed via the
      // suppressed property.
      assertEq(e instanceof CustomError, true);
    }
  }
  testGeneratorDoesntExposeMagicValue();
  assertArrayEq(values, [1, 2, "a"]);
  assertEq(caught, true);
}
