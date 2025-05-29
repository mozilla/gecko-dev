// |jit-test| skip-if: getBuildConfiguration("arm64-simulator") === true
// This test times out in ARM64 simulator builds.

// The test is also very slow with GenerationalGC zeal mode, especially if
// CheckHeapBeforeMinorGC is also enabled. GenerationalGC makes the number of
// minor GCs go 93 -> 126000.
if (this.unsetgczeal) {
    unsetgczeal("GenerationalGC");
}

function makeIonCompiledScript(n) {
  let src = "";
  for (var i = 0; i < n; i++) {
    src += "\n";
  }
  src += "function f() {}";
  eval(src);
  f();
  return f;
}

for (var i = 0; i < 5010; i++) {
  makeIonCompiledScript(i);
}
