// |jit-test| --fast-warmup; --fuzzing-safe; --gc-zeal=6,120

a = "(function f() { (function() { 0 "
for (b = 0; b < 50; b++) {
    c = `
        for (let [, d] = (() => {
                e = 10
                return [0, e]})();
            (() => {
                d--
                v26 = d
                return v26})();
            )
    `
    a += c + b
}
a += "})(); return  })"
eval(a)()

