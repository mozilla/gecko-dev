// |jit-test| --no-threads; --no-baseline; --no-ion;

var x = newGlobal({ newCompartment: true });
x.parent = this;
x.eval("(function(){ Debugger(parent); })()");

oomTest(function () {
  new WebAssembly.Instance(
    new WebAssembly.Module(
      wasmTextToBinary("(func (local i32) i32.const 0 local.set 0)"),
    ),
  );
});

