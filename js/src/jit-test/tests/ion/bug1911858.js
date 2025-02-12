// |jit-test| --fast-warmup; --no-threads
function f(x, y) {
    if (!Array.isArray(x)) {
        y = [y];
    }
    gc();
}
for (var i = 0; i < 100; i++) {
    [1, 2, 3].sort(f);
}
