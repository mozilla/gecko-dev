// |jit-test| --fuzzing-safe; --no-threads; --no-blinterp; --no-baseline; --no-ion

if (!('oomTest' in this)) {
  return;
}

g = newGlobal();
g.eval(`
Int8Array;
evalcx('\
  /x/;\
  oomTest(function() {\
    eval("function b() {}; function* f() {}");\
  });\
', newGlobal());
`);
