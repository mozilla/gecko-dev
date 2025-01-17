// |jit-test| --setpref=experimental.error_capture_stack_trace; --no-threads; --fast-warmup;
load(libdir + "asserts.js");

if ('captureStackTrace' in Error) {
    function caller(f) {
        return f();
    }


    function fill() {
        let x = {}
        Error.captureStackTrace(x, caller);
        return x;
    }

    let iter = 1500
    for (let i = 0; i < iter; i++) {
        // Make sure caller is an IonFrame.
        caller(fill);
    }


    function test_jit_elision() {
        let x = caller(fill);
        let { stack } = x;
        print(stack);
        assertEq(stack.includes("caller"), false);
        assertEq(stack.includes("fill"), false);
    }

    function test_jit_elision2() {
        ({ stack } = caller(() => {
            let x = caller(fill);
            return x;
        }));
        assertEq(stack.includes("caller"), true); // Only elide the first caller!
        assertEq(stack.includes("fill"), false);
    }

    test_jit_elision();
    test_jit_elision2();
}
