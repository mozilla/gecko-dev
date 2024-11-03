// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

class CustomError extends Error {}

{
  const values = [];
  let caught = false;
  async function* gen() {
    yield await Promise.resolve(1);
    await using a = {
      [Symbol.asyncDispose]() {
        values.push("a");
        throw new CustomError();
      }
    }
    yield await Promise.resolve(2);
    yield await Promise.resolve(3);
  }
  async function testGeneratorDoesntExposeMagicValue() {
    let it = gen();
    values.push((await it.next()).value);
    values.push((await it.next()).value);
    it.return(40).catch(e => {
      caught = true;
      // The "generator closing" is represented as a throw completion with a magic value.
      // The magic value shouldn't be captured as a suppressed error and exposed via the
      // suppressed property.
      assertEq(e instanceof CustomError, true);
    });
  }
  testGeneratorDoesntExposeMagicValue();
  drainJobQueue();
  assertArrayEq(values, [1, 2, "a"]);
  assertEq(caught, true);
}
