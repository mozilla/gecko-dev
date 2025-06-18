// |jit-test| --no-fjcvtzs; skip-if: !hasDisassembler() || !getBuildConfiguration("arm64") || !this.getJitCompilerOptions() || !getJitCompilerOptions()['ion.enable']

function checkAssembly() {
  let output = disnative(f);
  if (/backend=ion/.test(output)) {
    assertEq(/fcvtzs/.test(output), true);
    assertEq(/fjcvtzs/.test(output), false);
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
