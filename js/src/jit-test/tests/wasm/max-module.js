// |jit-test| --wasm-compiler=ion;

load(libdir + "wasm-binary.js");

let bytecode = [];
for (let i = 0; i < 1000; i++) {
  bytecode.push(CallCode);
  bytecode.push(...varU32(0));
}
let funcDef = funcBody({locals: [], body: bytecode});
let codeSectionSize = 512 * 1024 * 1024;
let numFuncs = Math.floor(codeSectionSize / funcDef.length);

if (numFuncs > 1000000) {
  throw new Error();
}

let decls = [];
let defs = [];
for (let i = 0; i < numFuncs; i++) {
  decls.push(0);
  defs.push(funcDef);
}

console.log('create module');
let binary = moduleWithSections([
    sigSection([{ args: [], ret: [] }]),
    declSection(decls),
    bodySection(defs)
  ]);

console.log('compile module');
new WebAssembly.Module(binary);
