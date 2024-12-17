// |jit-test| --fast-warmup; --no-threads


function f() {
    for (let i = 0; i < 9; i++) {
        for (let [j] = [0]; j < 1; j++) { }
    }
}
f();
disblic(f);
