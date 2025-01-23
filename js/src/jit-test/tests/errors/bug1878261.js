// |jit-test| --baseline-eager; skip-if: !hasFunction["gczeal"]

var g = newGlobal({newCompartment: true});

function foo() {
  try {
    g.eval("gczeal(7,1); throw 'a thrown string'");
  } finally {
    gczeal(0);
  }
}

try {
  foo();
} catch (e) { assertEq(e, 'a thrown string')}
