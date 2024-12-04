// |jit-test| error: TypeError

var global = newGlobal({ cloneSingletons: true });

var code = cacheEntry(`
function f() {}
Object.freeze(this);
`);

evaluate(code, { global, saveBytecodeWithDelazifications: true });
evaluate(code, { global, saveBytecodeWithDelazifications: true });
