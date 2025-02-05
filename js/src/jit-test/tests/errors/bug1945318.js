// |jit-test| skip-if: getBuildConfiguration("release_or_beta"); --setpref=experimental.error_capture_stack_trace
function not_called() {

}

let obj = {};
function* a() {
    yield {}; // Need to be past initial yield
    Error.captureStackTrace(obj, not_called);
}

async function b() {
    let g = a();
    await g.next();
    await g.next();
}

b().then(() => {
    assertEq('stack' in obj, true);
    assertEq(obj.stack, "");
})
