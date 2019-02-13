// |jit-test| test-join=--no-unboxed-objects
//
// Unboxed object optimization might not trigger in all cases, thus we ensure
// that Scalar Replacement optimization is working well independently of the
// object representation.

// Ion eager fails the test below because we have not yet created any
// template object in baseline before running the content of the top-level
// function.
if (getJitCompilerOptions()["ion.warmup.trigger"] <= 30)
    setJitCompilerOption("ion.warmup.trigger", 30);

// This test checks that we are able to remove the getelem & setelem with scalar
// replacement, so we should not force inline caches, as this would skip the
// generation of getelem & setelem instructions.
if (getJitCompilerOptions()["ion.forceinlineCaches"])
    setJitCompilerOption("ion.forceinlineCaches", 0);

var uceFault = function (j) {
    if (j >= 31)
        uceFault = function (j) { return true; };
    return false;
}

function f(j) {
    var i = Math.pow(2, j) | 0;
    var obj = {
      i: i,
      v: i + i
    };
    assertRecoveredOnBailout(obj, false); // :TODO: Fixed by Bug 1165348
    assertRecoveredOnBailout(obj.v, false); // :TODO: Fixed by Bug 1165348
    if (uceFault(j) || uceFault(j)) {
        // MObjectState::recover should neither fail,
        // nor coerce its result to an int32.
        assertEq(obj.v, 2 * i);
    }
    return 2 * obj.i;
}

var min = -100;
for (var j = min; j <= 31; ++j) {
    with({}){};
    f(j);
}
