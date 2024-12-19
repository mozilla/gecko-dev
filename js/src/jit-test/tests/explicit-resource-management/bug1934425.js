// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

async function foo() {
  let resources = [
    { [Symbol.asyncDispose]: () => 0},
    0
  ];
  try {
    for (await using d of resources);
  } catch {}
}
foo();
