oomTest(() => {
  let binary = wasmTextToBinary("(elem $e func 4)(func)(func)(func)(func)(func)");
  let module = new WebAssembly.Module(binary);
  let instance = new WebAssembly.Instance(module);
});
