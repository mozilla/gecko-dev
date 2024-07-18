// |jit-test| --gc-zeal=14,162;

a = function() {
    function b(c) {
      try {
        a();
      } catch {
      }
    }
    return b;
}()

d = wasmTextToBinary("(type $x (struct))(func $h)");
a();
wasmDumpIon(d, d);
