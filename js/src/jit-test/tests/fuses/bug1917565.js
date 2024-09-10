// |jit-test| --ion-offthread-compile=off; --fuzzing-safe; slow; skip-if: getBuildConfiguration("tsan")
setJitCompilerOption("ion.warmup.trigger", 5)

oomTest(function () {
    function foo() {
        for (a = 0; a < 10; a++) trialInline(..."use strict")
    }
    evaluate(foo + "foo()")
})
