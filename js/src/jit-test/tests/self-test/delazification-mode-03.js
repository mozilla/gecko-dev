let source = `
  function foo() {
    return "foo";
  }

  // Wait is skipped as the source is not registered in the stencil cache.
  waitForDelazificationOf(foo);
  assertEq(isDelazificationPopulatedFor(foo), false);
`;

const options = {
    fileName: "inner-03.js",
    lineNumber: 1,
    eagerDelazificationStrategy: "ParseEverythingEagerly",
    newContext: true,
};
evaluate(source, options);
