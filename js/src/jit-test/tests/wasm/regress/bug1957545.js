function f() {
  WebAssembly.instantiate(wasmTextToBinary("(func)")).catch(e => {});
  oomTest(f);
}
f();


