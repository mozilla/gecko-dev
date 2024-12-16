var bin = wasmTextToBinary(`
    (func $f)
    (table (export "table") 1 funcref)
    (elem (i32.const 0) $f)
`);
var inst = new WebAssembly.Instance(new WebAssembly.Module(bin));
oomTest(() => inst.exports.table.get(0)());
