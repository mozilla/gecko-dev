// |jit-test| skip-if: !wasmIsSupported(); --gc-zeal=10
var mod = new WebAssembly.Module(wasmTextToBinary(`(func)`))
var inst = new WebAssembly.Instance(mod);
for (var i = 0; i < 2; i++) {
    newGlobal({sameZoneAs:this}).Debugger(this).findScripts();
}
