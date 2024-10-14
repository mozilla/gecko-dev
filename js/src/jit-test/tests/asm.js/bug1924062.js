function a() {
  "use asm"
  function b() {}
  return b;
}
// asm.js module.
try { disnative(a); } catch (e) {}
// asm.js exported function.
try { disnative(a()); } catch (e) {}
