// |jit-test| skip-if: !hasDisassembler() || !getBuildConfiguration("arm64")

function checkAssembly() {
  let output = disnative(f);
  if (/backend=ion/.test(output)) {
    assertEq(/fcvtzs/.test(output) != /fjcvtzs/.test(output), true);
  }
}

function f(x) {
  if (inIon()) {
    checkAssembly();
    return 0;
  }
  return x | 0;
}

let i = 1.5;
while (f(i += 1)) {};
