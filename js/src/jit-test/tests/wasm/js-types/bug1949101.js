// |jit-test| skip-if: !('Function' in WebAssembly)

assertErrorMessage(
  () => new WebAssembly.Function({ parameters: Array(2000).fill('i32'), results: [] }, () => {}),
  TypeError,
  "too many function parameters",
);
assertErrorMessage(
  () => new WebAssembly.Function({ parameters: [], results: Array(2000).fill('i32') }, () => {}),
  TypeError,
  "too many function results",
);
