oomTest(function () {
  new WebAssembly.Instance(
    new WebAssembly.Module(
      wasmTextToBinary(
        '(module (import "m" "f" (func $f) )(func call $f))',
      ),
    ),
    {m: {f: function () { }}},
  );
});
