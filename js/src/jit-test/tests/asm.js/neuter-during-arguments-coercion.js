// |jit-test| test-also-noasmjs
load(libdir + "asm.js");

function f(stdlib, foreign, buffer)
{
  "use asm";
  var i32 = new stdlib.Int32Array(buffer);
  function set(v)
  {
    v=v|0;
    i32[5] = v;
  }
  return set;
}
if (isAsmJSCompilationAvailable())
  assertEq(isAsmJSModule(f), true);

var i32 = new Int32Array(4096);
var buffer = i32.buffer;
var set = f(this, null, buffer);
if (isAsmJSCompilationAvailable())
  assertEq(isAsmJSFunction(set), true);

try
{
  set({ valueOf: function() { neuter(buffer, "same-data"); return 17; } });
}
catch (e) { /* if an exception thrown, swallow */ }
