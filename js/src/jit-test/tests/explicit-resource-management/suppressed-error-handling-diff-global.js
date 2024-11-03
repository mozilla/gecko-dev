// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  const g1 = newGlobal({ newCompartment: true });
  const g2 = newGlobal({ newCompartment: true });
  function testDifferentGlobalErrors() {
    const g1Error = g1.evaluate(`new Error("g1")`);
    const g2Error = g2.evaluate(`new Error("g2")`);
    using x = {
      [Symbol.dispose]() {
        disposed.push(1);
        throw g1Error;
      }
    }
    using y = {
      [Symbol.dispose]() {
        disposed.push(2);
        throw g2Error;
      }
    }
    throw new Error("g");
  }
  assertSuppressionChainErrorMessages(testDifferentGlobalErrors, [
    {ctor: g1.evaluate('Error'), message: 'g1'},
    {ctor: g2.evaluate('Error'), message: 'g2'},
    {ctor: Error, message: 'g'},
  ]);
}
