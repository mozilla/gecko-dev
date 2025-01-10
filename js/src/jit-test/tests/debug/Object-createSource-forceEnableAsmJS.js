// |jit-test| skip-if: !wasmDebuggingEnabled()

const text = `
function normalFunction() {
}
function asmJSModule(stdlib, foreign) {
  "use asm";
  function asmJSFunction(x, y) {
    x = x|0;
    y = y|0;
    return (x + y)|0;
  }
  return asmJSFunction;
}
`;

// Test the recompilation scenario.
for (const forceEnableAsmJS of [false, true]) {
  const g = newGlobal({ newCompartment: true });

  g.evaluate(text, {
    fileName: "test.js",
  });

  const asmJSFunction = g.asmJSModule(globalThis, null);
  assertEq(asmJSFunction(2, 3), 5);

  const dbg = Debugger();
  const gdbg = dbg.addDebuggee(g);

  gc();

  // The top-level script should be GC-ed.
  const topLevelScriptObject = dbg.findScripts().find(
    s => s.format == "js" && !s.isFunction);
  assertEq(topLevelScriptObject, undefined);

  // asmJSModule should be seen as WasmInstanceObject.
  const asmJSModuleObject = dbg.findScripts().find(
    s => s.format == "wasm");
  assertEq(!!asmJSModuleObject, true);

  const source = gdbg.createSource({
    text,
    url: "test.js",
    startLine: 1,
    forceEnableAsmJS,
  });

  const asmJSModuleJSObject = dbg.findScripts().find(
    s => s.format == "js" && s.displayName == "asmJSModule");
  if (forceEnableAsmJS) {
    // If asm.js is force-enabled, createSource should enable the asm.js feature
    // and the asmJSModule function should be compiled as a asm.js module again,
    // and there shouldn't be BaseScript.
    assertEq(asmJSModuleJSObject, undefined);
  } else {
    // If asm.js is not force-enabled, createSource should disable the asm.js
    // feature, and the asmJSModule function should be compiled BaseScript.
    assertEq(!!asmJSModuleJSObject, true);
  }
}

// Test the initial compilation scenario.
for (const forceEnableAsmJS of [false, true]) {
  const g = newGlobal({ newCompartment: true });

  const dbg = Debugger();
  const gdbg = dbg.addDebuggee(g);

  const source = gdbg.createSource({
    text,
    url: "test.js",
    startLine: 1,
    forceEnableAsmJS,
  });

  const asmJSModuleJSObject = dbg.findScripts().find(
    s => s.format == "js" && s.displayName == "asmJSModule");
  if (forceEnableAsmJS) {
    // If asm.js is force-enabled, createSource should enable the asm.js feature
    // and the asmJSModule function should be compiled as a asm.js module, and
    // there shouldn't be BaseScript.
    assertEq(asmJSModuleJSObject, undefined);
  } else {
    // If asm.js is not force-enabled, createSource should disable the asm.js
    // feature, and the asmJSModule function should be compiled BaseScript.
    assertEq(!!asmJSModuleJSObject, true);
  }

  // If asm.js is not force-enabled, there shouldn't be WasmInstanceObject.
  //
  // Even if asm.js is force-enabled, the WasmInstanceObject is created only
  // when the asmJSModule function is called, and there shouldn't be
  // WasmInstanceObject yet.
  const asmJSModuleObject = dbg.findScripts().find(
    s => s.format == "wasm");
  assertEq(asmJSModuleObject, undefined);
}
