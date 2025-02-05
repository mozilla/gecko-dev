// |jit-test| skip-if: getBuildConfiguration("release_or_beta"); --setpref=experimental.error_capture_stack_trace;

function test_async_reparenting_with_filter() {
    let obj = {};
    let err = undefined;

    async function caller(callee) {
        return callee();
    }


    async function* a() {
        yield {};
        Error.captureStackTrace(obj, caller)
        err = new Error;
        yield {};
    }

    async function b() {
        return { a: 10 }
    }


    b().then(async () => {
        caller(async () => {
            let g = a();
            await g.next();
            await g.next();
            await g.next();
        })
    }).then(() => {
        // Note: No browser is re-parenting a stack and then filtering.
        //       So Safari & Chrome both report empty stacks for obj, where
        //       the stack obtained through Error is re-parented.
        //
        // Chrome 132:
        // Error
        //    at a (<anonymous>:12:11)
        //    at a.next (<anonymous>)
        //    at <anonymous>:24:17
        //
        // Safari
        //  @
        //  @
        //
        console.log("Capture: ")
        console.log(obj.stack);
        console.log("Stack")
        console.log(err.stack);
        assertEq(obj.stack, "");
        assertEq(err.stack == "", false);
    })
}
test_async_reparenting_with_filter();



function test_async_reparenting_without_filter() {
    let obj = {};
    let err = undefined;

    async function caller(callee) {
        return callee();
    }


    async function* a() {
        yield {};
        Error.captureStackTrace(obj)
        err = new Error;
        yield {};
    }

    async function b() {
        return { a: 10 }
    }


    b().then(async () => {
        caller(async () => {
            let g = a();
            await g.next();
            await g.next();
            await g.next();
        })
    }).then(() => {
        // Note: In the shell we re-parent this stack and so get the same
        // stack as the Error getter.
        console.log("Capture: ")
        console.log(obj.stack);
        console.log("Stack")
        console.log(err.stack);
        assertEq(obj.stack.length, err.stack.length);
    })
}
test_async_reparenting_without_filter();
