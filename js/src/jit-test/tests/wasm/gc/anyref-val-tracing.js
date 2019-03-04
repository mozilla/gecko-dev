// |jit-test| skip-if: !wasmReftypesEnabled()

gczeal(14, 1);
let { exports } = wasmEvalText(`(module
    (gc_feature_opt_in 3)
    (global $anyref (import "glob" "anyref") anyref)
    (func (export "get") (result anyref) global.get $anyref)
)`, {
    glob: {
        anyref: { sentinel: "lol" },
    }
});
assertEq(exports.get().sentinel, "lol");
