//|jit-test| skip-if: isLcovEnabled() || helperThreadCount() === 0

// GCs might trash the stencil cache. Prevent us from scheduling too many GCs.
if ('gczeal' in this) {
    gczeal(0);
}

let source = `
  function foo() {
    return "foo";
  }

  waitForDelazificationOf(foo);
  // false would be expected if threads are disabled.
  assertEq(isDelazificationPopulatedFor(foo), true);
`;

const options = {
    fileName: "inner-01.js",
    lineNumber: 1,
    eagerDelazificationStrategy: "CheckConcurrentWithOnDemand",
    newContext: true,
};
evaluate(source, options);
