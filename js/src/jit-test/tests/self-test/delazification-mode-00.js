//|jit-test| skip-if: isLcovEnabled()

let source = `
  function foo() {
    return "foo";
  }

  // Wait is skipped as the source is not registered in the stencil cache.
  waitForStencilCache(foo);
  assertEq(isInStencilCache(foo), false);
`;

const options = {
    fileName: "inner-00.js",
    lineNumber: 1,
    eagerDelazificationStrategy: "OnDemandOnly",
    newContext: true,
};
evaluate(source, options);
