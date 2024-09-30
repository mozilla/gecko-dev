// |jit-test| --fast-warmup; --no-threads; exitstatus: 6
timeout(0.05);
function f() {
    var b = "".match();
    try {
        f();
    } catch {}
}
f();
// Make sure we trigger the timeout to ensure the exitstatus is 6.
while (true) {}
